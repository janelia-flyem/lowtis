#include "DVIDBlockFetch.h"
#include <libdvid/DVIDNodeService.h>
#include <unordered_map>
#include "BlockCache.h"
#include <lowtis/lowtis.h>

using namespace lowtis; using namespace libdvid;
using std::string; using std::vector; using std::unordered_map;

DVIDBlockFetch::DVIDBlockFetch(DVIDConfig& config) :
        labeltypename(config.datatypename), usehighiopquery(config.usehighiopquery),
        node_service(config.dvid_server, config.dvid_uuid, config.username, "lowtis"),
	supervoxelview(config.supervoxelview)
{
    size_t isoblksize = node_service.get_blocksize(labeltypename); 
    blocksize = std::make_tuple(isoblksize, isoblksize, isoblksize);
    bytedepth = config.bytedepth;

    Json::Value typeinfo = node_service.get_typeinfo(labeltypename);
    
    // grayscale and labelarray use specific blocks interface
    // only supports grayscale jpeg and lz4/labelarray/labelmap labels
    dvidtype = typeinfo["Base"]["TypeName"].asString();
    if ((dvidtype == "labelarray") || (dvidtype == "labelmap")) {
        maxlevel = typeinfo["Extended"]["MaxDownresLevel"].asInt();
        usespecificblocks = true;
        compression_type = DVIDCompressedBlock::gzip_labelarray;
    } else if (bytedepth == 1) {
        usespecificblocks = true;
        string compression_string = typeinfo["Base"]["Compression"].asString();
        if (compression_string.find("LZ4") == 0) {
            compression_type = DVIDCompressedBlock::uncompressed;
        } else {
            compression_type = DVIDCompressedBlock::jpeg;
        }
    }  else {
        compression_type = DVIDCompressedBlock::lz4;
    }
}
    
void DVIDBlockFetch::prefetch_blocks(vector<libdvid::DVIDCompressedBlock>& blocks, int zoom)
{
    // only support prefetch for grayscale blocks now
    if (bytedepth != 1) {
        return;
    }

    if (blocks.empty()) {
        return;
    }

    vector<int> blockcoords;
        for (auto iter = blocks.begin(); iter != blocks.end(); ++iter) {
            vector<int> offset = iter->get_offset();
            size_t blocksize = iter->get_blocksize();
            blockcoords.push_back(offset[0]/blocksize);
            blockcoords.push_back(offset[1]/blocksize);
            blockcoords.push_back(offset[2]/blocksize);
        }
        string dataname_temp = labeltypename;
        if (zoom > 0) {
            dataname_temp += "_" + std::to_string(zoom);
        }

        node_service.prefetch_specificblocks3D(dataname_temp, blockcoords);   
}

vector<libdvid::DVIDCompressedBlock> DVIDBlockFetch::extract_blocks(
        vector<unsigned int> dims, vector<int> offset, int zoom)
{
    if ((dvidtype == "labelarray") || (dvidtype == "labelmap")) {
        throw LowtisErr("labelarray or labelmap only should use specific block interface");
    }
    size_t isoblksize = std::get<0>(blocksize);
    // make block aligned dims and offset
    int modoffset = offset[0] % isoblksize;
    offset[0] -= modoffset;
    dims[0] += modoffset;

    modoffset = offset[1] % isoblksize;
    offset[1] -= modoffset;
    dims[1] += modoffset;

    modoffset = offset[2] % isoblksize;
    offset[2] -= modoffset;
    dims[2] += modoffset;
   
    int modsize = offset[0] % isoblksize;
    if (modsize != 0) {
        dims[0] += (isoblksize - modsize);
    } 

    modsize = offset[1] % isoblksize;
    if (modsize != 0) {
        dims[1] += (isoblksize - modsize);
    } 
    
    modsize = offset[2] % isoblksize;
    if (modsize != 0) {
        dims[2] += (isoblksize - modsize);
    } 

    // call libdvid
    libdvid::Dims_t bdims;
    bdims.push_back(dims[0]);
    bdims.push_back(dims[1]);
    bdims.push_back(dims[2]);

    string dataname_temp = labeltypename;
    if (zoom > 0) {
        // do not use if explicit zoom levels supported
        dataname_temp += "_" + std::to_string(zoom);
    }

    // use grayscale interface for single byte
    // TODO: eventually replace with single libdvid call
    if (bytedepth == 1) {
        vector<libdvid::DVIDCompressedBlock> blocks = node_service.get_grayblocks3D(dataname_temp,
                bdims, offset, false);
        return blocks;
    } else {
        vector<libdvid::DVIDCompressedBlock> blocks = node_service.get_labelblocks3D(dataname_temp,
                bdims, offset, false);
        return blocks;
    }
}

// the zoom level is needed to interact with cache and set proper data source
void DVIDBlockFetch::extract_specific_blocks(
            vector<libdvid::DVIDCompressedBlock>& blocks, int zoom)
{
    if (blocks.empty()) {
        return;
    }
    vector<DVIDCompressedBlock> newblocks; 

    // only grayscale fetch works with specific block interface
    if (!usehighiopquery || (!usespecificblocks)) {
        // perform if labelblk datatype (>1 byte and lz4 compression)
        // find bounding box for request 
        int minx = INT_MAX; int miny = INT_MAX; int minz = INT_MAX;
        int maxx = INT_MIN; int maxy = INT_MIN; int maxz = INT_MIN;

        for (auto iter = blocks.begin(); iter != blocks.end(); ++iter) {
            vector<int> offset = iter->get_offset();
            size_t blocksize = iter->get_blocksize();
            if (offset[0] < minx) {
                minx = offset[0];
            }
            if (offset[1] < miny) {
                miny = offset[1];
            }
            if (offset[2] < minz) {
                minz = offset[2];
            }
            if (int(offset[0]+blocksize) > maxx) {
                maxx = offset[0]+blocksize;
            }
            if (int(offset[1]+blocksize) > maxy) {
                maxy = offset[1]+blocksize;
            }
            if (int(offset[2]+blocksize) > maxz) {
                maxz = offset[2]+blocksize;
            }
        }
        vector<int> goffset;
        goffset.push_back(minx);
        goffset.push_back(miny);
        goffset.push_back(minz);

        vector<unsigned int> dims;
        dims.push_back(maxx-minx);
        dims.push_back(maxy-miny);
        dims.push_back(maxz-minz);

        // call main query function
        // TODO: allow parallel requests to decompose an inefficiently packed bbox
        newblocks = extract_blocks(dims, goffset, zoom); 
    } else {
        vector<int> blockcoords;
        for (auto iter = blocks.begin(); iter != blocks.end(); ++iter) {
            vector<int> offset = iter->get_offset();
            size_t blocksize = iter->get_blocksize();
            blockcoords.push_back(offset[0]/blocksize);
            blockcoords.push_back(offset[1]/blocksize);
            blockcoords.push_back(offset[2]/blocksize);
        }
        string dataname_temp = labeltypename;
        if ((zoom > 0) && (dvidtype != "labelarray") && (dvidtype != "labelmap")) {
            // do not use if explicit zoom levels supported
            dataname_temp += "_" + std::to_string(zoom);
        }
        
        // check against max scale level (easy to do for labelarray)
        if ((zoom > maxlevel) && ((dvidtype == "labelarray") || (dvidtype == "labelmap"))) {
            throw LowtisErr("Trying to request unknown scale level");
        }

        if ((dvidtype == "labelarray") || (dvidtype == "labelmap")) {
            // set scale for labelarray and labelmap
            node_service.get_specificblocks3D(dataname_temp, blockcoords, true, newblocks, zoom, false, supervoxelview);
        } else {
            if (compression_type == DVIDCompressedBlock::uncompressed) {
                //std::cout << "blah0" << std::endl;
                node_service.get_specificblocks3D(dataname_temp, blockcoords, true, newblocks, 0, true);
                //std::cout << "blah1" << std::endl;
            } else {
                node_service.get_specificblocks3D(dataname_temp, blockcoords, true, newblocks);
            }
        }
    }

    // find and set requested blocks
    unordered_map<BlockCoords, BlockData> cache;
    for (auto iter = newblocks.begin(); iter != newblocks.end(); ++iter) {
        BlockCoords coords;
        vector<int> offset = iter->get_offset();
        coords.x = offset[0];
        coords.y = offset[1];
        coords.z = offset[2];
        coords.zoom = zoom;
        BlockData data;
        data.block = *iter;
        cache[coords] = data;
    }
    for (auto iter = blocks.begin(); iter != blocks.end(); ++iter) {
        BlockCoords coords;
        vector<int> offset = iter->get_offset();
        coords.x = offset[0];
        coords.y = offset[1];
        coords.z = offset[2];
        coords.zoom = zoom;

        auto dataiter = cache.find(coords);
        if (dataiter != cache.end()) {
            //throw LowtisErr("Failed to fetch block");
            iter->set_data(dataiter->second.block.get_data());
        }
    }
}

