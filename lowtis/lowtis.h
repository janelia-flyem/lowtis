#ifndef LOWTIS_H
#define LOWTIS_H

#include <lowtis/LowtisConfig.h>
#include <lowtis/BlockFetch.h>
#include <lowtis/BlockCache.h>
#include <libdvid/BinaryData.h>

namespace lowtis {

/*!
 * Main class to access 2D image data.  Requests cannot be made
 * in parallel to this class.  If more than one image is desired
 * at a time, another ImageService should be created.  Users
 * only need to understand this interface and the appropriate
 * configuration settings.
*/
class ImageService {
  public:
    /*!
     * Constructor will set configuration and create a
     * block fetcher and cache.
     * \param config_ configuration base class
    */ 
    ImageService(LowtisConfig& config_);


    // ?! make non-blocking call but block on main request
    // ?! TODO: background prefetch queue (priorities will change frequently)
    // ?! load data in callback provided in config
    // ?! grab bbox that needs to be called after examining cache
    // ?! call minimum libdvid calls and load in cache
    // ?! load data a call callback with buffer
    
    /*!
     * Retrieves image data.  This function is blocking and will
     * return some data.  Depending on the configuration, the
     * callback can be called asynchronously to update the view.
     * \param viewport size of window
     * \param offset offset of image
     * \param zoom zoom level (0 is full zoom)
    :*/ 
    libdvid::BinaryDataPtr retrieve_image(unsigned int width,
        unsigned int height, std::vector<int> offset, int zoom=0);

    /*!
     * Pause future requests and asynchronous calls.
    */
    void pause();

    /*!
     * Empties the cache.  This should be called if previous segmentation
     * has been invalidated and the cache should be reset.
    */
    void flush_cache();

  private:
    // ?! pre-allocated buffer after first size request  
  
    //! interface to fetch block data
    BlockFetchPtr fetcher;

    //! configuration for lowtis
    LowtisConfig& config; 

    //! class lock
    std::mutex gmutex;

    //! pause mode
    bool paused = false;

    //! holds block data cache
    BlockCache cache;
};

}

#endif
