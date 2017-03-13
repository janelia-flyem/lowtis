#include "GoogleBlockFetch.h"
#include "BlockCache.h"
#include <functional>

#include <sstream>
#include <json/json.h>

using namespace lowtis; using namespace libdvid;
using std::string; using std::vector; using std::unordered_map;
using std::shared_ptr;
using std::stringstream;
using std::min; using std::max;
using std::ifstream;

struct FetchData {
    FetchData(DVIDNodeService& service_, string request_, DVIDCompressedBlock& block_,
                int zoom_, Geometry relshifted_, bool withinvol_,
                unordered_map<BlockCoords, BlockData>& cache,   
                int& threads_remaining_, boost::mutex& m_mutex_,
                boost::condition_variable& m_condition_) : service(service_), request(request_),
                block(block_), zoom(zoom_), relshifted(relshifted_), withinvol(withinvol_),
                cache(cache), threads_remaining(threads_remaining_), m_mutex(m_mutex_),
                m_condition(m_condition_) {} 

    void operator()()
    {
        auto bdata = service.custom_request(request, BinaryDataPtr(), GET); 

        BlockData data;
        if (withinvol) {
            // just copy to cache
            block.set_data(bdata);
            data.block = block;
        } else {
            // fill-in block border and compress to lz4
            unsigned int width, height;
            bdata = BinaryData::decompress_jpeg(bdata, width, height);

            auto offset = block.get_offset();

            // make sure volume size matches jpeg size
            size_t volsize = (relshifted.xmax - relshifted.xmin + 1) *
                (relshifted.ymax - relshifted.ymin + 1) *
                (relshifted.zmax - relshifted.zmin + 1); 
            assert((width*height == volsize));

            // copy data to new buffer
            size_t blocksize = block.get_blocksize();
            char *buffer = new char[blocksize*blocksize*blocksize]();

            int ptrpos = 0;
            const unsigned char* rawdata = bdata->get_raw();
            for (int z = relshifted.zmin; z <= relshifted.zmax; ++z) {
                for (int y = relshifted.ymin; y <= relshifted.ymax; ++y) {
                    size_t glbpos = (z)*(blocksize*blocksize) +
                        (y)*blocksize + relshifted.xmin;
                    for (int x = relshifted.xmin; x <= relshifted.xmax; ++x) {
                        buffer[glbpos] = rawdata[ptrpos];
                        ++glbpos;
                        ++ptrpos;
                    }
                }
            }
            auto finaldata = BinaryData::create_binary_data(buffer, blocksize*blocksize*blocksize);

            // re-compress data (now lz4) and save to block         
            finaldata = BinaryData::compress_lz4(finaldata);
            DVIDCompressedBlock cblock(finaldata, block.get_offset(), blocksize, block.get_typesize(), DVIDCompressedBlock::lz4);

            data.block = cblock;
        }

        // load data into shared block cache
        BlockCoords coords;
        vector<int> offset = block.get_offset();
        coords.x = offset[0];
        coords.y = offset[1];
        coords.z = offset[2];
        coords.zoom = zoom;

        boost::mutex::scoped_lock lock(m_mutex);
        cache[coords] = data;
        threads_remaining--;
        //m_condition.notify_one();
    }

    DVIDNodeService service;
    string request;
    DVIDCompressedBlock block;
    int zoom;
    Geometry relshifted;
    bool withinvol;
    unordered_map<BlockCoords, BlockData>& cache;
    int& threads_remaining;
    boost::mutex& m_mutex;
    boost::condition_variable& m_condition;
};


GoogleBlockFetch::GoogleBlockFetch(GoogleGrayblkConfig& config) :
        node_service(config.dvid_server, config.dvid_uuid, config.username, "lowtis") 
{
    // setup default params
    bytedepth = config.bytedepth;
    datatypename = config.datatypename;
    assert(bytedepth == 1);
    blocksize = std::make_tuple(config.isoblksize, config.isoblksize, config.isoblksize);
    compression_type = DVIDCompressedBlock::jpeg;
  
    // extract geometry 
    auto metadata = node_service.custom_request(config.datatypename + "/info", BinaryDataPtr(), GET); 
    Json::Value data;
    Json::Reader json_reader;
    if (!json_reader.parse(metadata->get_data(), data)) {
        throw ErrMsg("Could not decode JSON");
    }

    // max zoom level
    maxlevel = data["Extended"]["Scales"].size() - 1;
    
    // find highest resolution level
    geometry.xmin = 0;
    geometry.ymin = 0;
    geometry.zmin = 0;
    int smallestres = INT_MAX; 
    for (unsigned int i = 0; i <= maxlevel; ++i) {
        int pixres = data["Extended"]["Scales"][i]["pixelSize"][0].asUInt();
        if (pixres < smallestres) {
            geometry.xmax = data["Extended"]["Scales"][i]["volumeSize"][0].asUInt();
            geometry.ymax = data["Extended"]["Scales"][i]["volumeSize"][1].asUInt();
            geometry.zmax = data["Extended"]["Scales"][i]["volumeSize"][2].asUInt();
            smallestres = pixres;
        }
    } 
}

// the zoom level is needed to interact with cache and set proper data source
void GoogleBlockFetch::extract_specific_blocks(
            vector<libdvid::DVIDCompressedBlock>& blocks, int zoom)
{
    if (blocks.empty()) {
        return;
    }

    std::unordered_map<BlockCoords, BlockData> cache;
    
    int multiplier = 1;
    for (int i = 0; i < zoom; ++i) {
        multiplier *= 2;
    }

    // setup thread pool
    //GoogleThreadPool* pool = GoogleThreadPool::get_pool();
    
    boost::thread_group* threads; // destructor auto deletes threads
    threads = new boost::thread_group;
    int threads_remaining = blocks.size();
    int num_launched = 0;
    const int MAXSIMULT = 50; // some crashes in DVID on mac with a lot of requests

    for (auto iter = blocks.begin(); iter != blocks.end(); ++iter) {
        // check extents
        int blocksize = iter->get_blocksize() * multiplier;
        vector<int> offset = iter->get_offset();
      
        // shift offset back
        offset[0] *= multiplier;
        offset[1] *= multiplier;
        offset[2] *= multiplier;

        // if outside of extents completely, just skip (will be blank)
        if ((offset[0] > geometry.xmax) ||
            (offset[1] > geometry.ymax) ||
            (offset[2] > geometry.ymax) ||
            ((offset[0]+blocksize-1) < 0) ||
            ((offset[1]+blocksize-1) < 0) ||
            ((offset[2]+blocksize-1) < 0)) {
            
            boost::mutex::scoped_lock lock(m_mutex);
            --threads_remaining;
            continue;
        }

        // check if overlap and create local geometry
        Geometry georig;
        georig.xmin = offset[0];
        georig.ymin = offset[1];
        georig.zmin = offset[2];
        georig.xmax = offset[0] + blocksize - 1;
        georig.ymax = offset[1] + blocksize - 1;
        georig.zmax = offset[2] + blocksize - 1;

        Geometry relshifted; // relative shift within downloaded block if needed
        bool withinvol = true;
        if ((offset[0] < 0) ||
            (offset[1] < 0) ||
            (offset[2] < 0) ||
            ((offset[0]+blocksize-1) > geometry.xmax) ||
            ((offset[1]+blocksize-1) > geometry.ymax) ||
            ((offset[2]+blocksize-1) > geometry.zmax) ) {
            withinvol = false;

            georig.xmin = max(offset[0], 0);
            georig.ymin = max(offset[1], 0);
            georig.zmin = max(offset[2], 0);
            georig.xmax = min(offset[0] + blocksize - 1, int(geometry.xmax));
            georig.ymax = min(offset[1] + blocksize - 1, int(geometry.ymax));
            georig.zmax = min(offset[2] + blocksize - 1, int(geometry.zmax));
        
            // put into rel coordinate system
            relshifted.xmin = (georig.xmin-offset[0]) / multiplier;
            relshifted.ymin = (georig.ymin-offset[1]) / multiplier;
            relshifted.zmin = (georig.zmin-offset[2]) / multiplier;
            relshifted.xmax = (georig.xmax - georig.xmin + 1) / multiplier - 1;
            relshifted.ymax = (georig.ymax - georig.ymin + 1) / multiplier - 1;
            relshifted.zmax = (georig.zmax - georig.zmin + 1) / multiplier - 1;
        } 

        // construct query string
        stringstream url;
        url << datatypename << "/raw/0_1_2/";
        url << (georig.xmax - georig.xmin + 1) / multiplier << "_" 
            << (georig.ymax - georig.ymin + 1) / multiplier << "_"
            << (georig.zmax - georig.zmin + 1) / multiplier  << "/";
        url << georig.xmin/multiplier << "_" << georig.ymin/multiplier << "_" << georig.zmin/multiplier;
        url << "/jpg:80?scale=" << zoom;

        // launch thread with url
        //pool->add_task(FetchData(node_service, url.str(), *iter, zoom, relshifted,
          //          withinvol, cache, threads_remaining, m_mutex, m_condition));


        boost::thread* t = new boost::thread(FetchData(node_service, url.str(), *iter, zoom, relshifted,  withinvol, cache, threads_remaining, m_mutex, m_condition));
        threads->add_thread(t);
        //FetchData blah(node_service, url.str(), *iter, zoom, relshifted,  withinvol, cache, threads_remaining, m_mutex, m_condition);
        //blah();
        ++num_launched;

        if (num_launched > MAXSIMULT) {
            threads->join_all();
            num_launched = 0;
            delete threads;
            threads = new boost::thread_group;
        }
    }

    // wait for threads to finish
    threads->join_all();
    delete threads;

    // load data into missing blocks  
    
    vector<libdvid::DVIDCompressedBlock> tblocks = blocks;
    blocks.clear();
    for (auto iter = tblocks.begin(); iter != tblocks.end(); ++iter) {
        BlockCoords coords;
        vector<int> offset = iter->get_offset();
        coords.x = offset[0];
        coords.y = offset[1];
        coords.z = offset[2];
        coords.zoom = zoom;

        auto dataiter = cache.find(coords);
        if (dataiter != cache.end()) {
            blocks.push_back(dataiter->second.block);
        } else {
            blocks.push_back(*iter);
        }
    }
}

