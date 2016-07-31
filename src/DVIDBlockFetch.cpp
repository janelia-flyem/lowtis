#include "DVIDBlockFetch.h"
#include "BlockCache.h"
#include <libdvid/DVIDNodeService.h>

using namespace lowtis; using namespace libdvid;
using std::string; using std::vector; using std::unordered_map;

DVIDBlockFetch::DVIDBlockFetch(DVIDConfig& config) :
        labeltypename(config.datatypename),
        node_service(config.dvid_server, config.dvid_uuid, config.username, "lowtis") 
{
   blocksize = node_service.get_blocksize(labeltypename); 
}

vector<libdvid::DVIDCompressedBlock> DVIDBlockFetch::extract_blocks(
        vector<unsigned int> dims, vector<int> offset)
{
    // make block aligned dims and offset
    int modoffset = offset[0] % blocksize;
    offset[0] -= modoffset;
    dims[0] += modoffset;

    modoffset = offset[1] % blocksize;
    offset[1] -= modoffset;
    dims[1] += modoffset;

    modoffset = offset[2] % blocksize;
    offset[2] -= modoffset;
    dims[2] += modoffset;
   
    int modsize = offset[0] % blocksize;
    if (modsize != 0) {
        dims[0] += (blocksize - modsize);
    } 

    modsize = offset[1] % blocksize;
    if (modsize != 0) {
        dims[1] += (blocksize - modsize);
    } 
    
    modsize = offset[2] % blocksize;
    if (modsize != 0) {
        dims[2] += (blocksize - modsize);
    } 

    // call libdvid
    libdvid::Dims_t bdims;
    bdims.push_back(dims[0]);
    bdims.push_back(dims[1]);
    bdims.push_back(dims[2]);
    vector<libdvid::DVIDCompressedBlock> blocks = node_service.get_labelblocks3D(labeltypename,
           bdims, offset, false);
    return blocks;
}

void DVIDBlockFetch::extract_specific_blocks(
            vector<libdvid::DVIDCompressedBlock>& blocks)
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
    vector<DVIDCompressedBlock> newblocks = extract_blocks(dims, goffset); 

    // find and set requested blocks
    unordered_map<BlockCoords, BlockData> cache;
    for (auto iter = newblocks.begin(); iter != newblocks.end(); ++iter) {
        BlockCoords coords;
        vector<int> offset = iter->get_offset();
        coords.x = offset[0];
        coords.y = offset[1];
        coords.z = offset[2];
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

        auto dataiter = cache.find(coords);
        if (dataiter != cache.end()) {
            //throw LowtisErr("Failed to fetch block");
            iter->set_data(dataiter->second.block.get_data());
        }
    }
}

vector<libdvid::DVIDCompressedBlock> DVIDBlockFetch::intersecting_blocks(
        vector<unsigned int> dims, vector<int> offset)
{  
    // make block aligned dims and offset
    int modoffset = offset[0] % blocksize;
    offset[0] -= modoffset;
    dims[0] += modoffset;

    modoffset = offset[1] % blocksize;
    offset[1] -= modoffset;
    dims[1] += modoffset;

    modoffset = offset[2] % blocksize;
    offset[2] -= modoffset;
    dims[2] += modoffset;
   
    int modsize = dims[0] % blocksize;
    if (modsize != 0) {
        dims[0] += (blocksize - modsize);
    } 

    modsize = dims[1] % blocksize;
    if (modsize != 0) {
        dims[1] += (blocksize - modsize);
    } 
    
    modsize = dims[2] % blocksize;
    if (modsize != 0) {
        dims[2] += (blocksize - modsize);
    } 

    vector<libdvid::DVIDCompressedBlock> blocks;
    libdvid::BinaryDataPtr emptyptr(0);

    for (int z = 0; z < (dims[2]/blocksize); ++z) {
        for (int y = 0; y < (dims[1]/blocksize); ++y) {
            for (int x = 0; x < (dims[0]/blocksize); ++x) {
                vector<int> toffset;
                toffset.push_back(offset[0]+x*blocksize);
                toffset.push_back(offset[1]+y*blocksize);
                toffset.push_back(offset[2]+z*blocksize);
                libdvid::DVIDCompressedBlock cblock(emptyptr, toffset,
                        blocksize, sizeof(libdvid::uint64));
                blocks.push_back(cblock);
            }
        }
    }

    return blocks;
}


