#include "DVIDBlockFetch.h"
#include <libdvid/DVIDNodeService.h>
#include <unordered_map>
#include "BlockCache.h"

using namespace lowtis; using namespace libdvid;
using std::string; using std::vector; using std::unordered_map;

DVIDBlockFetch::DVIDBlockFetch(DVIDConfig& config) :
        labeltypename(config.datatypename),
        node_service(config.dvid_server, config.dvid_uuid, config.username, "lowtis") 
{
    size_t isoblksize = node_service.get_blocksize(labeltypename); 
    blocksize = std::make_tuple(isoblksize, isoblksize, isoblksize);
    bytedepth = config.bytedepth;
    // only supports grayscale jpeg and lz4 labels
    if (bytedepth == 1) {
        compression_type = DVIDCompressedBlock::jpeg;
    } else {
        compression_type = DVIDCompressedBlock::lz4;
    }
}

vector<libdvid::DVIDCompressedBlock> DVIDBlockFetch::extract_blocks(
        vector<unsigned int> dims, vector<int> offset, int zoom)
{
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
    vector<DVIDCompressedBlock> newblocks = extract_blocks(dims, goffset, zoom); 

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

