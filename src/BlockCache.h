#ifndef BLOCKCACHE_H
#define BLOCKCACHE_H

#include <unordered_map>
#include <boost/functional/hash.hpp>
#include <functional>
#include <time.h>
#include <mutex>
#include <libdvid/DVIDBlocks.h>

namespace lowtis {
struct BlockCoords {
    int x = 0;
    int y = 0;
    int z = 0;

    bool operator==(const BlockCoords& coord2) const
    {
        return x == coord2.x && y == coord2.y && z == coord2.z;
    }
};
}

namespace std {
template <>
struct hash<lowtis::BlockCoords> {
    std::size_t operator()(const lowtis::BlockCoords& coords) const
    {
        std::size_t seed = 0;
        boost::hash_combine(seed, coords.x);
        boost::hash_combine(seed, coords.y);
        boost::hash_combine(seed, coords.z);
        return seed; 
    }
};

}

namespace lowtis {

struct BlockData {
    //BlockData() = default;
    //! hold actual compressed data TODO: make custom chunk object
    libdvid::DVIDCompressedBlock block;
    time_t timestamp;
};

/*!
 * Caches chunks of image data.  This class is responsible
 * for fast indexing of data, evicting old cache data, limiting
 * the size of the cache.  It is not responsible for interpreting
 * the cached data.  Each function is thread-safe; we assume that
 * time waiting for locks will be insignificant compared to data
 * fetch time.
*/ 
class BlockCache {
  public:
    BlockCache() {}
    
    BlockCache(size_t max_size_, size_t time_limit_);

    /*!
     * Sets time threshold for invalidating the cache.  set to 0
     * will turn off the timer.
     * \param seconds time in seconds between timer event.
    */
    void set_timer(int seconds);

    /*!
     * Sets max size of cache
     * \param max_size_ size limit in MBs
    */ 
    void set_max_size(size_t max_size_);

    /*!
     * Empty the cache.
    */
    void flush() { cache.clear(); }
        
    /*!
     * Fetches a block if it exists and is recent as defined
     * by the user specified time limit.
     * \param coords coordinates for block
     * \param block value (if found) for block
     * \return true if found block, false otherwise
    */
    bool retrieve_block(BlockCoords coords, libdvid::DVIDCompressedBlock& block);
    void set_block(libdvid::DVIDCompressedBlock block);

  private:
    
    /*!
     * Remove cache entries before the specified timestamp.
     * \param time_thresholed cutoff to remove old entries
    */
    void remove_old_entries(time_t time_threshold);
    
    /*!
     * Shrink size of cache in half.
    */
    void shrink_cache();
    
    //!! cache of blocks (indexed by coordinates)
    std::unordered_map<BlockCoords, BlockData> cache;

    //! max size of cache in MBs
    size_t max_size = 2000;

    //! size of db (in bytes)
    unsigned long long curr_cache_size = 0;

    //! time limit in seconds till eviction (0 is no eviction)
    size_t time_limit = 0;

    std::mutex gmutex;

};


}

#endif
