#ifndef LOWTISCONFIG_H
#define LOWTISCONFIG_H

#include <string>
#include <memory>
#include <tuple>

namespace lowtis {

struct LowtisConfig;
typedef std::shared_ptr<LowtisConfig> LowtisConfigPtr;

/*!
 * Options for image service.  Compile with c++11.
*/
struct LowtisConfig {
    LowtisConfig(size_t bytedepth_) : bytedepth(bytedepth_) {}

    //! duration to keep items in cache (in seconds) (0 = no time limit)
    unsigned int refresh_rate = 120;
    
    //! cache limit (in MBs)
    // TODO: make dynamic?? 
    unsigned int cache_size = 1000;
 
    //! uncompressed cache limit (in MBs) -- default off 
    unsigned int uncompressed_cache_size = 0;

    //! default value of empty block
    unsigned char emptyval = 0;    

    //! user calling program
    std::string username = "anonymous"; 
    
    //! number of bytes per pixel
    size_t bytedepth = 1; 
    
    //! size of middle region to fetch first (0,0) means make
    //! the call non-blocking
    std::tuple<int, int> centercut;
    
    //! enables prefetching for blocks
    //! (no-op, server side, local depending on the fetcher)
    bool enableprefetch = false;
    
    // ?! add callback here

    virtual ~LowtisConfig() {}
};


/*!
 * Configuration settings if using DVID back-end.
*/
struct DVIDConfig : public LowtisConfig {
    DVIDConfig(size_t bytedepth_) : LowtisConfig(bytedepth_) {}
    std::string dvid_server;
    std::string dvid_uuid;
    std::string datatypename;
    bool supervoxelview = false;
    bool usehighiopquery = true;
};

struct DVIDGrayblkConfig : public DVIDConfig {
    DVIDGrayblkConfig() : DVIDConfig(1)
    {
        refresh_rate = 0;
        uncompressed_cache_size = 1000;
    }
};


struct DVIDLabelblkConfig : public DVIDConfig {
    DVIDLabelblkConfig() : DVIDConfig(8) {
        uncompressed_cache_size = 1000;
    }
};


/*!
 * Configuration settings if using Google brainmaps back-end.
*/
struct GoogleGrayblkConfig : public DVIDConfig {
    GoogleGrayblkConfig() : DVIDConfig(1)
    {
        refresh_rate = 0;
        uncompressed_cache_size = 1000;
    }
    size_t isoblksize = 64;
};

}

#endif 
