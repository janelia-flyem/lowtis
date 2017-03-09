#include <lowtis/lowtis.h>
#include "BlockFetch.h"
#include "BlockFetchFactory.h"
#include "BlockCache.h"
#include <boost/thread/thread.hpp>
#include <time.h>
#include <chrono>

using namespace lowtis;
using namespace libdvid;
using std::vector;
using std::shared_ptr;
using std::ostream;
using std::get;

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

void ImageService::flush_cache()
{
    gmutex.lock();
    cache->flush();
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

void ImageService::retrieve_image(unsigned int width,
        unsigned int height, vector<int> offset, char* buffer, int zoom, bool centercut)
{
    if (!centercut) {
        gmutex.lock();
        _retrieve_image(width, height, offset, buffer, zoom, fetcher);
        gmutex.unlock();
    } else {
        // call as boost threads and join
        boost::thread_group threads;

        // retrieve high-resolution center
        unsigned int cwidth = get<0>(config.centercut);
        unsigned int cheight = get<0>(config.centercut);
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
        tempoffset[0] += offset0;
        tempoffset[1] += offset1;

        gmutex.lock();
        boost::thread* t1 = new boost::thread(&ImageService::_retrieve_image, this, cwidth, cheight, tempoffset, buffer2, zoom, fetcher);
        threads.add_thread(t1);

        boost::thread* t2 = new boost::thread(&ImageService::_retrieve_image, this, width/2, height/2, offset, buffer3, zoom+1, fetcher2);
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
            char * bufferiter = buffer + ((j+(height-cheight)/2)*width*config.bytedepth) + (width-cwidth)/2;
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
        unsigned int height, vector<int> offset, char* buffer, int zoom, shared_ptr<BlockFetch> curr_fetcher)
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
    vector<DVIDCompressedBlock> blocks = curr_fetcher->intersecting_blocks(dims, offset);

    // check cache and save missing blocks
    vector<DVIDCompressedBlock> current_blocks;
    vector<DVIDCompressedBlock> missing_blocks;

    for (auto iter = blocks.begin(); iter != blocks.end(); ++iter) {
        BlockCoords coords;
        vector<int> toffset = iter->get_offset();
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
        int num_threads = 8; // ?! do dynamically

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
    
    auto final_time = std::chrono::high_resolution_clock::now(); 
    //std::cout << "tile time: " << std::chrono::duration_cast<std::chrono::milliseconds>(final_time-initial_time).count() << " milliseconds" << std::endl;
}




ostream& operator<<(ostream& os, LowtisErr& err)
{
    os << err.what(); 
    return os;
}



