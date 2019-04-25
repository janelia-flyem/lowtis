#ifndef GOOGLEBLOCKFETCH_H
#define GOOGLEBLOCKFETCH_H

#include "BlockFetch.h"
#include "BlockCache.h"
#include <lowtis/LowtisConfig.h>
#include <mutex>
#include <unordered_map>
#include <iostream>
#include <libdvid/DVIDNodeService.h>

#include <boost/asio/io_service.hpp>
#include <boost/bind.hpp>
#include <boost/thread/thread.hpp>

namespace lowtis {

/*!
 * Thread pool singleton for threading resources.
*/
class GoogleThreadPool {
  public:
    static GoogleThreadPool* get_pool()
    {
        static GoogleThreadPool pool;
        return &pool;
    }

    template <typename TTask>
    void add_task(TTask task)
    {
        std::cout << "in1" << std::endl;
        io_service_.dispatch(task);
    }
    ~GoogleThreadPool()
    {
        delete work_ctrl_;
    }

  private:
    GoogleThreadPool()
    {
        std::cout << "create" << std::endl;
        work_ctrl_ = new boost::asio::io_service::work(io_service_);
        int workers = 20; // harded-coded for now 
        for (int i = 0; i < workers; ++i) {
            std::cout << "thread: " << i << std::endl;
            threads_.create_thread(boost::bind(&boost::asio::io_service::run, &io_service_));
        }
        std::cout << "done" << std::endl;
    }

    boost::asio::io_service io_service_;
    boost::thread_group threads_;
    boost::asio::io_service::work *work_ctrl_;

};

struct Geometry {
    int xmin, ymin, zmin;
    int xmax, ymax, zmax;
};

class GoogleBlockFetch : public BlockFetch {
  public:
    /*! Setup authentication and extract volume geometry.
     * \param config contains server, uuid, user, datatypename, blocksize
     */
    GoogleBlockFetch(GoogleGrayblkConfig& config);

    /*!
     * Base class virtual function for retrieving blocks specified.
     * \param blocks loads compressed block data into provided coordinates 
    */
    void extract_specific_blocks(
            std::vector<libdvid::DVIDCompressedBlock>& blocks, int zoom);

  private:
    boost::mutex m_mutex;
    boost::condition_variable m_condition;
    
    Geometry geometry;
    std::string datatypename;
    libdvid::DVIDNodeService node_service;
    size_t maxlevel = 0;
};

}

#endif
