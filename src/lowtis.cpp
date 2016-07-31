#include <lowtis/lowtis.h>
#include "BlockFetch.h"
#include "BlockFetchFactory.h"
#include "BlockCache.h"

using namespace lowtis;
using namespace libdvid;
using std::vector;
using std::shared_ptr;
using std::ostream;

ImageService::ImageService(LowtisConfig& config_) : config(config_)
{
    fetcher = create_blockfetcher(&config_);
    cache = shared_ptr<BlockCache>(new BlockCache);
    cache->set_timer(config.refresh_rate);
    cache->set_max_size(config.cache_size);
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

void ImageService::retrieve_image(unsigned int width,
        unsigned int height, vector<int> offset, char* buffer, int zoom)
{
    gmutex.lock(); // do I need to lock the entire function?

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
    vector<DVIDCompressedBlock> blocks = fetcher->intersecting_blocks(dims, offset);

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

    // call interface for blocks desired 
    fetcher->extract_specific_blocks(missing_blocks, zoom);
    // concatenate block lists
    current_blocks.insert(current_blocks.end(), missing_blocks.begin(), 
            missing_blocks.end());

    for (auto iter = missing_blocks.begin(); iter != missing_blocks.end(); ++iter) {
        cache->set_block(*iter, zoom);
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
    gmutex.unlock();
}

ostream& operator<<(ostream& os, LowtisErr& err)
{
    os << err.what(); 
    return os;
}



