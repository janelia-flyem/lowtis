#ifndef LOWTISCONFIG_H
#define LOWTISCONFIG_H

#include <memory>

namespace lowtis {

class LowtisConfig;    
typedef std::shared_ptr<LowtisConfig> LowtisConfigPtr;

/*!
 * Options for image service.  Compile with c++11.
*/
struct LowtisConfig {
    LowtisConfigPtr creater_pointer()
    {
        return LowtisConfigPtr(new LowtisConfig(*this));
    }
    
    //! duration to keep items in cache (in seconds)
    unsigned int refresh_rate = 120;
    
    //! cache limit (in MBs) (not using yet)
    // TODO: make dynamic?? 
    unsigned int cache_size = 1000;

    size_t bytedepth = 8; 
   

    std::string username = "anonymous"; 
    // ?! add callback here

    virtual ~LowtisConfig() {}
};


/*!
 * Configuration settings if using DVID back-end.
*/
struct DVIDConfig : public LowtisConfig {
    LowtisConfigPtr creater_pointer()
    {
        return LowtisConfigPtr(new DVIDConfig(*this));
    }
    
    std::string dvid_server;
    std::string dvid_uuid;
    std::string datatypename;
};

}

#endif 
