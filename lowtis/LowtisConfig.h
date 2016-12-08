#ifndef LOWTISCONFIG_H
#define LOWTISCONFIG_H

#include <string>
#include <memory>

namespace lowtis {

class LowtisConfig;    
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
  
    //! default value of empty block 
    unsigned char emptyval = 0;    

    //! user calling program
    std::string username = "anonymous"; 
    
    //! number of bytes per pixel
    size_t bytedepth = 1; 
    
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
};

struct DVIDGrayblkConfig : public DVIDConfig {
    DVIDGrayblkConfig() : DVIDConfig(1)
    {
        refresh_rate = 0;
    }
};

struct DVIDLabelblkConfig : public DVIDConfig {
    DVIDLabelblkConfig() : DVIDConfig(8) {}
};


}

#endif 
