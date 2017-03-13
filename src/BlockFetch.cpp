#include "BlockFetch.h"
#include <libdvid/DVIDNodeService.h>
#include <unordered_map>

using namespace lowtis; using namespace libdvid;
using std::string; using std::vector; using std::unordered_map;

vector<libdvid::DVIDCompressedBlock> BlockFetch::intersecting_blocks(
        vector<unsigned int> dims, vector<int> offset)
{  
    // ?! temporary simplifying hack
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
   
    int modsize = dims[0] % isoblksize;
    if (modsize != 0) {
        dims[0] += (isoblksize - modsize);
    } 

    modsize = dims[1] % isoblksize;
    if (modsize != 0) {
        dims[1] += (isoblksize - modsize);
    } 
    
    modsize = dims[2] % isoblksize;
    if (modsize != 0) {
        dims[2] += (isoblksize - modsize);
    } 

    vector<libdvid::DVIDCompressedBlock> blocks;
    libdvid::BinaryDataPtr emptyptr(0);

    for (int z = 0; z < (dims[2]/isoblksize); ++z) {
        for (int y = 0; y < (dims[1]/isoblksize); ++y) {
            for (int x = 0; x < (dims[0]/isoblksize); ++x) {
                vector<int> toffset;
                toffset.push_back(offset[0]+x*isoblksize);
                toffset.push_back(offset[1]+y*isoblksize);
                toffset.push_back(offset[2]+z*isoblksize);
                libdvid::DVIDCompressedBlock cblock(emptyptr, toffset,
                        isoblksize, bytedepth, compression_type);
                blocks.push_back(cblock);
            }
        }
    }

    return blocks;
}
