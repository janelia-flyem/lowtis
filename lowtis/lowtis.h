#ifndef LOWTIS_H
#define LOWTIS_H

#include <lowtis/LowtisConfig.h>

#include <mutex>
#include <memory>
#include <vector>

namespace lowtis {

struct BlockCache;
struct BlockFetch;

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

    /*!
     * Retrieves image data for a fixed orientation (determined by the config file
     * if supported by lowtis DB driver .  This function is blocking and will
     * return data in provided buffer.
     * \param viewport size of window
     * \param offset offset of image
     * \param buffer preallocated image buffer (size: height*width*bytedepth)
     * \param zoom power of two zoom level (0 is full zoom)
    */ 
    void retrieve_image(unsigned int width,
        unsigned int height, std::vector<int> offset, char* buffer, int zoom=0, bool centercut=false);

    /*!
     * Retrieves image data.  This function is blocking and will
     * return some data. 
     * Note: dim1, dim2 must be orthogonal.
     * \param viewport size of window
     * \param centerloc location of image center
     * \param dim1vec gives dim1 orientation vector 
     * \param dim2vec gives dim2 orientation vector 
     * \param buffer preallocated image buffer (size: height*width*bytedepth)
     * \param zoom power of two zoom level (0 is full zoom)
    */ 
    void retrieve_arbimage(unsigned int width,
        unsigned int height, std::vector<int> centerloc, std::vector<double> dim1vec,
        std::vector<double> dim2vec, char* buffer, int zoom=0, bool centercut=false);

    /*!
     * Pause future requests and asynchronous calls.
    */
    void pause();

    /*!
     * Empties the cache.  This should be called if previous segmentation
     * has been invalidated and the cache should be reset.
    */
    void flush_cache();

    /*
     * Changes center cut configuration.
    */
    void set_centercut(const std::tuple<int, int>& centercut);

  private:
    /*!
     * Retrieve image plane.  If no plane is defined, it just
     * defaults to the Z plane.
    */
    void _retrieve_image_fovea(unsigned int width,
        unsigned int height, std::vector<int> offset, char* buffer, int zoom, bool centercut, std::vector<double> dim1step, std::vector<double> dim2step);

    void _retrieve_image(unsigned int width,
        unsigned int height, std::vector<int> offset, char* buffer, int zoom, std::shared_ptr<BlockFetch> curr_fetcher, std::vector<double> dim1step, std::vector<double> dim2step);
    
    //! interface to fetch block data
    std::shared_ptr<BlockFetch> fetcher;
    
    //! interface to fetch low-res block data
    std::shared_ptr<BlockFetch> fetcher2;

    //! configuration for lowtis
    LowtisConfig config; 

    //! class lock
    std::mutex gmutex;
    
    //! pause mode
    bool paused = false;

    //! holds block data cache
    std::shared_ptr<BlockCache> cache;
    
    //! holds decompressed block data cache (when decompression is slow)
    std::shared_ptr<BlockCache> uncompressed_cache;
};

/*!
 * Exception handling for general lowtis errors.
 * It is a wrapper for simple string message.
*/
class LowtisErr : public std::exception { 
  public:
    /*!
     * Construct takes a string message for the error.
     * \param msg_ string message
    */
    explicit LowtisErr(std::string msg_) : msg(msg_) {}
    
    /*!
     * Implement exception base class function.
    */
    virtual const char* what() const throw()
    {
        return msg.c_str();
    }

    /*!
     * Empty destructor.
    */
    virtual ~LowtisErr() throw() {}
  protected:
    
    //! Error message
    std::string msg;
};

/*!
 * Function that allows formatting of error to standard output.
*/
std::ostream& operator<<(std::ostream& os, LowtisErr& err);

}

#endif
