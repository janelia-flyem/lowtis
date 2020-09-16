#include <lowtis/lowtis.h>
#include "BlockFetch.h"
#include "BlockFetchFactory.h"
#include "BlockCache.h"
#include <boost/thread/thread.hpp>
#include <thread>
#include <time.h>
#include <chrono>
#include <cmath>

using namespace lowtis;
using namespace libdvid;
using std::vector;
using std::shared_ptr;
using std::ostream;
using std::get;
using std::sqrt;
using std::round;
using std::unordered_map;

ImageService::ImageService(LowtisConfig& config_) : config(config_)
{
    fetcher = create_blockfetcher(&config_);
    fetcher2 = create_blockfetcher(&config_);
    cache = shared_ptr<BlockCache>(new BlockCache);
    cache->set_timer(config.refresh_rate);
    cache->set_max_size(config.cache_size);

    if (config.uncompressed_cache_size > 0) {
        uncompressed_cache = shared_ptr<BlockCache>(new BlockCache);
        uncompressed_cache->set_timer(config.refresh_rate);
        uncompressed_cache->set_max_size(config.uncompressed_cache_size);
    }
}

void ImageService::pause()
{
    gmutex.lock();
    paused ^= 1;
    gmutex.unlock();
}

void ImageService::set_centercut(const std::tuple<int, int>& centercut)
{
    config.centercut = centercut;
}


void ImageService::flush_cache()
{
    gmutex.lock();
    cache->flush();
    if (uncompressed_cache) {
        uncompressed_cache->flush();
    }
    gmutex.unlock();
}

void decompress_block(vector<DVIDCompressedBlock>* blocks, int id, int num_threads, int zoom, shared_ptr<BlockCache> uncompressed_cache)
{
    BinaryDataPtr uncompressed_data;
    int curr_id = 0;

    for (auto iter = blocks->begin(); iter != blocks->end(); ++iter, ++curr_id) {
        if ((curr_id % num_threads) == id) {
            if ((iter->get_data())) {
                // check of block exists in uncompressed_cache 
                BlockCoords coords; 
                vector<int> toffset = iter->get_offset();
                coords.x = toffset[0];
                coords.y = toffset[1];
                coords.z = toffset[2];
                coords.zoom = zoom;

                DVIDCompressedBlock dblock = *iter;
                bool found = uncompressed_cache->retrieve_block(coords, dblock);
               
                if (!found) {
                    // check if already decompressed to avoid decompress
                    uncompressed_data = iter->get_uncompressed_data(); 
                    size_t bsize = iter->get_blocksize();
                    size_t tsize = iter->get_typesize();

                    DVIDCompressedBlock temp_block(uncompressed_data, toffset, bsize, tsize, DVIDCompressedBlock::uncompressed);
                    uncompressed_cache->set_block(temp_block, zoom);
                    (*blocks)[curr_id] = temp_block;
                } else {
                    (*blocks)[curr_id] = dblock;
                }
            }
        }
    }
}

// Update currloc based on vector step (just rounds to nearest integer location)
inline void increment_vector(vector<int>& currloc, vector<double>& dim1unitvec,
       vector<double>& dim2unitvec, vector<double>& dim3unitvec,
       int dim1incr, int dim2incr, int dim3incr)
{
    for (size_t i = 0; i < currloc.size(); ++i) {
        currloc[i] = round(currloc[i] + dim1unitvec[i]*dim1incr + dim2unitvec[i]*dim2incr + dim3unitvec[i]*dim3incr);
    }
}


void ImageService::retrieve_arbimage(unsigned int width, unsigned int height,
        vector<int> centerloc, vector<double> dim1vec, vector<double> dim2vec, char* buffer, int zoom,
        bool centercut)
{
    // check if roughly orthogonal
    assert(centerloc.size() == 3);
    assert(dim1vec.size() == 3);
    assert(dim2vec.size() == 3);
    double dotprod = 0;
    for (int i = 0; i < 3; ++i) {
        //dotprod += ((dim1vec[i] - centerloc[i])*(dim2vec[i] - centerloc[i]));
        dotprod += ((dim1vec[i])*(dim2vec[i]));
    }
    assert(dotprod > -0.0001 && dotprod < 0.0001);

    /*
    // find true width for zoom 
    for (int i = 0; i < zoom; i++) {
        width *= 2;
        height *= 2;
    }*/
    
    // calculate unit vector 
    vector<double> dim1step;
    vector<double> dim2step;
    double dim1norm = 0;
    double dim2norm = 0;
    for (int i = 0; i < 3; ++i) {
        //double val = dim1vec[i]-centerloc[i];
        double val = dim1vec[i];
        dim1norm += val*val;
        dim1step.push_back(val);
        
        //val = dim2vec[i]-centerloc[i];
        val = dim2vec[i];
        dim2norm += val*val;
        dim2step.push_back(val);
    }
    dim1norm = sqrt(dim1norm);
    dim2norm = sqrt(dim2norm);
    for (int i = 0; i < 3; ++i) {
        dim1step[i] /= dim1norm;
        dim2step[i] /= dim2norm;
    }

    // calculate offset in global coordinates along dim1 and dim2
    vector<int> offset = centerloc;
    vector<double> dummyvec(3,0);

    int offset0 = -1*int(width)/2;
    int offset1 = -1*int(height)/2;
    for (int i = 0; i < zoom; ++i) {
        offset0 *= 2;
        offset1 *= 2;
    }

    increment_vector(offset, dim1step, dim2step, dummyvec, offset0, offset1, 0);

    _retrieve_image_fovea(width, height, offset, buffer, zoom, centercut, dim1step, dim2step); 
}

void ImageService::retrieve_image(unsigned int width,
        unsigned int height, vector<int> offset, char* buffer, int zoom, bool centercut)
{
    vector<double> dim1step, dim2step;
    _retrieve_image_fovea(width, height, offset, buffer, zoom, centercut, dim1step, dim2step); 
}

void ImageService::_retrieve_image_fovea(unsigned int width,
        unsigned int height, vector<int> offset, char* buffer, int zoom, bool centercut, vector<double> dim1step, vector<double> dim2step)
{
    unsigned int cwidth = 0;
    unsigned int cheight = 0; 

    if (centercut) {
        // retrieve high-resolution center
        cwidth = get<0>(config.centercut);
        cheight = get<0>(config.centercut);

        // if either dimension is smaller than the center cut, disable centercut
        if ((cwidth >= width) || (cheight >= height)) {
            centercut = false;
        }
    }

    if (!centercut) {
        gmutex.lock();
        _retrieve_image(width, height, offset, buffer, zoom, fetcher, dim1step, dim2step);
        gmutex.unlock();
    } else {
        // call as boost threads and join
        boost::thread_group threads;

        char *buffer2 = new char[cwidth*cheight*config.bytedepth];
 
        // retrieve 1/4 image at lower resolution
        // !! this requires the caller to avoid using the fovia if at the lowest resolution already
        char *buffer3 = new char[width/2*height/2*config.bytedepth];

        // make new offset for small window
        vector<int> tempoffset = offset;
        int offset0 = (width-cwidth)/2;
        int offset1 = (height-cheight)/2;
        for (int i = 0; i < zoom; ++i) {
            offset0 *= 2;
            offset1 *= 2;
        }

        if (!dim1step.empty()) {
            vector<double> dummyvec(3,0);
            increment_vector(tempoffset, dim1step, dim2step, dummyvec, offset0, offset1, 0); 
        } else {
            tempoffset[0] += offset0;
            tempoffset[1] += offset1;
        }

        gmutex.lock();
        boost::thread* t1 = new boost::thread(&ImageService::_retrieve_image, this, cwidth, cheight, tempoffset, buffer2, zoom, fetcher, dim1step, dim2step);
        threads.add_thread(t1);

        boost::thread* t2 = new boost::thread(&ImageService::_retrieve_image, this, width/2, height/2, offset, buffer3, zoom+1, fetcher2, dim1step, dim2step);
        //_retrieve_image(width/2, height/2, offset, buffer3, zoom+1, fetcher2);
        threads.add_thread(t2);
       
        // wait for results 
        threads.join_all();
        gmutex.unlock();
       
        // set buffers
        // write low resolution version into buffer
        char * bufferiter = buffer;
        char * bufferiter2 = buffer;
        char* buffer3_iter = buffer3;
        for (int j = 0; j < height/2; j++) {
            char * bufferiter = buffer + (2*j*width*config.bytedepth);
            char * bufferiter2 = bufferiter + (width*config.bytedepth);
            for (int i = 0; i < width/2; i++) {
                for (int iter = 0; iter < config.bytedepth; iter++) {
                    // write into four spots (simple downsample)
                    *bufferiter = *buffer3_iter;
                    bufferiter[config.bytedepth] = *buffer3_iter;
                    ++bufferiter;

                    *bufferiter2 = *buffer3_iter;
                    bufferiter2[config.bytedepth] = *buffer3_iter;
                    ++bufferiter2;

                    ++buffer3_iter;
                }
                bufferiter += (config.bytedepth);
                bufferiter2 += (config.bytedepth);
            }
        } 
        
        // write center cut
        char* buffer2_iter = buffer2;
        for (int j = 0; j < cheight; ++j) {
            char * bufferiter = buffer + ((j+(height-cheight)/2)*width*config.bytedepth) + (width-cwidth)/2*config.bytedepth;
            for (int i = 0; i < cwidth; ++i) {
                for (int iter = 0; iter < config.bytedepth; iter++) {
                    *bufferiter = *buffer2_iter;
                    
                    ++buffer2_iter;
                    ++bufferiter;
                }
            }
        }

        // TODO: keep a memory buffer to avoid reallocation (already know centercut size)
        delete []buffer2;
        delete []buffer3;
    }
}

void ImageService::_retrieve_image(unsigned int width,
        unsigned int height, vector<int> offset, char* buffer, int zoom, shared_ptr<BlockFetch> curr_fetcher, vector<double> dim1step, vector<double> dim2step)
{
    auto initial_time = std::chrono::high_resolution_clock::now(); 
    // adjust offset for zoom
    for (int i = 0; i < zoom; i++) {
        offset[0] /= 2;
        offset[1] /= 2;
        offset[2] /= 2;
    }

    // find intersecting blocks
    // TODO: make 2D call instead (remove dims and say dim1, dim2)
    vector<unsigned int> dims;
    dims.push_back(width);
    dims.push_back(height);
    dims.push_back(1);

    auto start_cache_time = std::chrono::high_resolution_clock::now();
    vector<double> dim3step(3, 0); // only will work on a dim1, dim2 
    vector<DVIDCompressedBlock> blocks = curr_fetcher->intersecting_blocks(dims, offset, dim1step, dim2step, dim3step);
    // check cache and save missing blocks
    vector<DVIDCompressedBlock> current_blocks;
    vector<DVIDCompressedBlock> missing_blocks;

    for (auto iter = blocks.begin(); iter != blocks.end(); ++iter) {
        BlockCoords coords;
        const vector<int>& toffset = iter->get_offset();
        coords.x = toffset[0];
        coords.y = toffset[1];
        coords.z = toffset[2];
        coords.zoom = zoom;
        
        DVIDCompressedBlock block = *iter;
        bool found = cache->retrieve_block(coords, block);
        if (found) {
            current_blocks.push_back(block);
        } else {
            missing_blocks.push_back(block);
        }
    }

    auto end_cache_time = std::chrono::high_resolution_clock::now();
    //std::cout << "cache time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end_cache_time - start_cache_time).count() << " milliseconds" << std::endl;

    // fetch data    
    auto start_fetch_time = std::chrono::high_resolution_clock::now(); 
    
    // call interface for blocks desired 
    curr_fetcher->extract_specific_blocks(missing_blocks, zoom);
    
    auto end_fetch_time = std::chrono::high_resolution_clock::now(); 
    //std::cout << "fetch time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end_fetch_time-start_fetch_time).count() << " milliseconds" << std::endl;

    current_blocks.insert(current_blocks.end(), missing_blocks.begin(), 
            missing_blocks.end());

    // add missing blocks to regular cache 
    for (auto iter = missing_blocks.begin(); iter != missing_blocks.end(); ++iter) {
        cache->set_block(*iter, zoom);
    }

    // decompress blocks if necessary
    if (uncompressed_cache) { 
        auto ct1 = std::chrono::high_resolution_clock::now(); 

        boost::thread_group threads; // destructor auto deletes threads
        int num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) {
            // just default to something if hardware concurrency not supported
            num_threads = 8;
        }
        

        vector<boost::thread*> curr_threads;  
        for (int i = 0; i < num_threads; ++i) {
            boost::thread* t = new boost::thread(decompress_block, &current_blocks, i, num_threads, zoom, uncompressed_cache);
            threads.add_thread(t);
            curr_threads.push_back(t);
        } 
        threads.join_all();
        
        auto ct2 = std::chrono::high_resolution_clock::now(); 
        //std::cout << "decompress: " << std::chrono::duration_cast<std::chrono::milliseconds>(ct2-ct1).count() << " milliseconds" << std::endl;
    }
    
    // TODO: better arbitrary cut interpolation (ideally would also change intersection algorithm)
    auto start_compute_intersection_time = std::chrono::high_resolution_clock::now();
    if (!dim1step.empty()) {
        // create lookup map for blocks
        unordered_map<BlockCoords, const unsigned char* > mappedblocks;
        for (auto iter = current_blocks.begin(); iter != current_blocks.end(); ++iter) {
            const vector<int>& toffset = iter->get_offset();
            BlockCoords coords;
            coords.x = toffset[0];
            coords.y = toffset[1];
            coords.z = toffset[2];
            if (iter->get_data()) {
                mappedblocks[coords] = iter->get_uncompressed_data()->get_raw();
            } else {
                mappedblocks[coords] = 0;
            }
        }

        // !! assume uniform blocks
        size_t isoblksize = current_blocks[0].get_blocksize();
       
        // set default value for image 
        assert((config.emptyval == 0) || (config.bytedepth == 1));
        memset(buffer, config.emptyval, width*height*config.bytedepth);
        
        vector<double> toffset(3);
        toffset[0] = offset[0];
        toffset[1] = offset[1];
        toffset[2] = offset[2];

        const unsigned char* raw_data = nullptr;
        BlockCoords pre_coords;
        pre_coords.x = INT32_MIN;
        pre_coords.y = INT32_MIN;
        pre_coords.z = INT32_MIN;

        for (int dim2 = 0; dim2 < height; ++dim2) {
            for (int dim1 = 0; dim1 < width; ++dim1) {
                // grab block address
                int x = static_cast<int>(toffset[0] + 0.5);
                int y = static_cast<int>(toffset[1] + 0.5);
                int z = static_cast<int>(toffset[2] + 0.5);
                // find offset within block
                int xshift = x % isoblksize;
                int yshift = y % isoblksize;
                int zshift = z % isoblksize;

                BlockCoords coords;
                coords.x = x - xshift;
                coords.y = y - yshift;
                coords.z = z - zshift;

                if (!(pre_coords == coords))
                {
                    raw_data = mappedblocks[coords];
                    pre_coords = coords;
                }

                // don't write data if empty
                if (raw_data) {
                    const unsigned char*  raw_data_local = raw_data +(zshift*(isoblksize*isoblksize) + yshift*isoblksize + xshift)*config.bytedepth;

                    for (int bytepos = 0; bytepos < config.bytedepth; ++bytepos) {
                        *buffer = *raw_data_local;

                        // write buffer in order
                        ++raw_data_local;
                        ++buffer;
                    }
                } else {
                    buffer += config.bytedepth;
                }

                toffset[0] += dim1step[0];
                toffset[1] += dim1step[1];
                toffset[2] += dim1step[2];
            }
            toffset[0] -= (width*dim1step[0]);
            toffset[1] -= (width*dim1step[1]);
            toffset[2] -= (width*dim1step[2]);

            toffset[0] += (dim2step[0]);
            toffset[1] += (dim2step[1]);
            toffset[2] += (dim2step[2]);
        }
    } else {
        // populate image from blocks and return data
        for (auto iter = current_blocks.begin(); iter != current_blocks.end(); ++iter) {
            bool emptyblock = false;
            if (!(iter->get_data())) {
                emptyblock = true;
            }

            size_t blocksize = iter->get_blocksize();

            const unsigned char* raw_data = 0;

            BinaryDataPtr raw_data_ptr;
            if (!emptyblock) {
                raw_data_ptr = iter->get_uncompressed_data();
                raw_data = raw_data_ptr->get_raw();
                if (raw_data_ptr->length() < blocksize * blocksize * blocksize) {
                    emptyblock = true;
                }
            }

            // extract common dim3 offset (will refer to as 'z')
            vector<int> toffset = iter->get_offset();
            int zoff = offset[2] - toffset[2];

            // find intersection between block and buffer
            int startx = std::max(offset[0], toffset[0]);
            int finishx = std::min(offset[0]+int(width), toffset[0]+int(blocksize));
            int starty = std::max(offset[1], toffset[1]);
            int finishy = std::min(offset[1]+int(height), toffset[1]+int(blocksize));

            unsigned long long iterpos = zoff * blocksize * blocksize * config.bytedepth;

            // point to correct plane
            raw_data += iterpos;

            // point to correct y,x
            raw_data += (((starty-toffset[1])*blocksize*config.bytedepth) + 
                    ((startx-toffset[0])*config.bytedepth)); 
            char* bytebuffer_temp = buffer + ((starty-offset[1])*width*config.bytedepth) +
                ((startx-offset[0])*config.bytedepth); 

            for (int ypos = starty; ypos < finishy; ++ypos) {
                for (int xpos = startx; xpos < finishx; ++xpos) {
                    for (int bytepos = 0; bytepos < config.bytedepth; ++bytepos) {
                        if (!emptyblock) {
                            *bytebuffer_temp = *raw_data;
                        } else {
                            // TODO: properly set for entire bytedepth
                            *bytebuffer_temp = config.emptyval;
                        }
                        ++raw_data;
                        ++bytebuffer_temp;
                    } 
                }
                raw_data += (blocksize-(finishx-startx))*config.bytedepth;
                bytebuffer_temp += (width-(finishx-startx))*config.bytedepth;
            }
        }
    }
    auto end_compute_intersection_time = std::chrono::high_resolution_clock::now();
    //std::cout << "compute intersection time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end_compute_intersection_time - start_compute_intersection_time).count() << " milliseconds" << std::endl;
   
    // perform non-blocking prefetch
    // depending on the block fetcher this will be either a non-opt,
    // a remote cache, or a local cache
    if (config.enableprefetch) {
        // TODO: allow prefetch size to be configured
        
        // prefetch is determined as the relative zoom level
        vector<unsigned int> dims;
        vector <int> newoffset = offset;
        // 10% increase to each side
        int newwidth = width + width/5;
        int newheight = height + height/5;
       
        // 10 planes above/below
        int newdepth = 21;
      
        vector<double> dim3step;
       
        if (!dim1step.empty()) {
            // compute dim3step by cross product
            dim3step.push_back(dim1step[1]*dim2step[2]+dim1step[2]*dim2step[1]); 
            dim3step.push_back(dim1step[2]*dim2step[0]+dim1step[0]*dim2step[2]);
            dim3step.push_back(dim1step[0]*dim2step[1]+dim1step[1]*dim2step[0]); 
             
            increment_vector(newoffset, dim1step, dim2step, dim3step, -width/10, -height/10, -10); 
        } else {
            // adjust offset
            newoffset[0] -= width/10; 
            newoffset[1] -= height/10; 
            newoffset[2] -= 10; 
        }

        
        dims.push_back(newwidth);
        dims.push_back(newheight);
        dims.push_back(newdepth);

        vector<DVIDCompressedBlock> blocks = curr_fetcher->intersecting_blocks(dims, newoffset, dim1step, dim2step, dim3step);

        // check cache and save missing blocks
        vector<DVIDCompressedBlock> missing_blocks;

        // only prefetch missing blocks
        for (auto iter = blocks.begin(); iter != blocks.end(); ++iter) {
            BlockCoords coords;
            vector<int> toffset = iter->get_offset();
            coords.x = toffset[0];
            coords.y = toffset[1];
            coords.z = toffset[2];
            coords.zoom = zoom;

            DVIDCompressedBlock block = *iter;
            bool found = cache->retrieve_block(coords, block);
            if (!found) {
                missing_blocks.push_back(block);
            }
        }

        // call non-blocking prefetcher (might no-op)
        curr_fetcher->prefetch_blocks(missing_blocks, zoom);
    }


    auto final_time = std::chrono::high_resolution_clock::now(); 
    //std::cout << "tile time: " << std::chrono::duration_cast<std::chrono::milliseconds>(final_time-initial_time).count() << " milliseconds" << std::endl;
}




ostream& operator<<(ostream& os, LowtisErr& err)
{
    os << err.what(); 
    return os;
}



