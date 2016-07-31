#include "BlockCache.h"
#include <vector>

using namespace lowtis;
using namespace libdvid;
using std::unordered_map; using std::nth_element; using std::vector;

BlockCache::BlockCache(size_t max_size_, size_t time_limit_) : 
    max_size(max_size_), time_limit(time_limit_), curr_cache_size(0) {}

void BlockCache::set_timer(int seconds)
{
    gmutex.lock();
    time_limit = seconds;
    gmutex.unlock();
}

void BlockCache::set_max_size(size_t max_size_)
{
    gmutex.lock();
    max_size = max_size_;
    gmutex.unlock();
}

bool BlockCache::retrieve_block(BlockCoords coords, DVIDCompressedBlock& block)
{
    gmutex.lock();
    bool found = false;
    auto cache_iter = cache.find(coords);
    if (cache_iter != cache.end()) {
        time_t current_time = time(0);
        if ( !time_limit ||
             ((current_time - cache_iter->second.timestamp) < time_limit) ) {
            block = cache_iter->second.block;
            found = true;
        }   
    }
    
    gmutex.unlock();
    return found;

}
    
void BlockCache::set_block(DVIDCompressedBlock block, int zoom)
{
    gmutex.lock();
   
    // insert new block
    if (block.get_data()) {
        curr_cache_size += block.get_datasize(); 
    }
    BlockData bdata;
    bdata.timestamp = time(0); 
    bdata.block = block; 
    BlockCoords coords;
    vector<int> offset = block.get_offset();
    coords.x = offset[0];
    coords.y = offset[1];
    coords.z = offset[2];
    coords.zoom = zoom;
    cache[coords] = bdata;
    
    // check if cache is full
    if (curr_cache_size/1000000 > max_size) {
        shrink_cache();
    }    
 
    gmutex.unlock();
}

// caller must lock cache
void BlockCache::shrink_cache()
{
    vector<time_t> timevec(cache.size());
    int pos = 0;
    for (auto iter = cache.begin(); iter != cache.end(); ++iter, ++pos) {
        timevec[pos] = iter->second.timestamp;
    }

    nth_element(timevec.begin(), timevec.begin()+timevec.size()/2, timevec.end());
    remove_old_entries(timevec[timevec.size()/2]);
}

// caller must lock cache
void BlockCache::remove_old_entries(time_t time_threshold)
{
    for (auto cache_iter = cache.begin(); cache_iter != cache.end();) {
        if (cache_iter->second.timestamp < time_threshold) {
            // points to next iterator position
            if (cache_iter->second.block.get_data()) {
                curr_cache_size -= cache_iter->second.block.get_datasize();
            }
            cache_iter = cache.erase(cache_iter);
        } else {
            ++cache_iter;
        }
    }
}


