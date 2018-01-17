#include "BlockFetch.h"
#include "BlockCache.h"
#include <libdvid/DVIDNodeService.h>
#include <unordered_map>
#include <unordered_set>
#include <cmath>

using namespace lowtis; using namespace libdvid;
using std::string; using std::vector; using std::unordered_map;
using std::round; using std::unordered_set;

vector<libdvid::DVIDCompressedBlock> BlockFetch::intersecting_blocks(
        vector<unsigned int> dims, vector<int> offset, vector<double> dim1step,
        vector<double> dim2step, vector<double> dim3step)
{  
    // ?! temporary simplifying hack
    size_t isoblksize = std::get<0>(blocksize);
    
    vector<libdvid::DVIDCompressedBlock> blocks;
    libdvid::BinaryDataPtr emptyptr(0);

    // need special loop to handle arbitrary plane
    // (more computationally expensive)
    if (!dim1step.empty()) {
        // duplicate blocks could be found
        // when checking arbitrary angles
        unordered_set<BlockCoords> foundblocks;
        
        BlockCoords savedcoords;
        savedcoords.x = INT_MAX;
        vector<double> toffset(3, 0);
        vector<int> toffset2(3, 0);
        for (int z = 0; z < dims[2]; ++z) {
            // set offset
            for (size_t i = 0; i < toffset.size(); ++i) {
                toffset[i] = offset[i] +  dim3step[i]*z;
            }

            for (int y = 0; y < dims[1]; ++y) {
                for (int x = 0; x < dims[0]; ++x) {
                    // find blocks for arbitrary angle
                    // avoid adding duplicate blocks
                    BlockCoords coords;
                    coords.x = round(toffset[0]) - (int(round(toffset[0])) % isoblksize);
                    coords.y = round(toffset[1]) - (int(round(toffset[1])) % isoblksize);
                    coords.z = round(toffset[2]) - (int(round(toffset[2])) % isoblksize);
                   
                    // if same as previous block, do not even lookup 
                    if (!(savedcoords == coords)) {
                        if (foundblocks.find(coords) == foundblocks.end()) {
                            foundblocks.insert(coords);
                            toffset2[0] = coords.x;
                            toffset2[1] = coords.y;
                            toffset2[2] = coords.z;
                            libdvid::DVIDCompressedBlock cblock(emptyptr, toffset2,
                                    isoblksize, bytedepth, compression_type);
                            blocks.push_back(cblock);
                        }
                        savedcoords = coords;
                    }

                    toffset[0] += dim1step[0];
                    toffset[1] += dim1step[1];
                    toffset[2] += dim1step[2];
                }
                    
                toffset[0] -= (dims[0]*dim1step[0]);
                toffset[1] -= (dims[0]*dim1step[1]);
                toffset[2] -= (dims[0]*dim1step[2]);

                toffset[0] += (dim2step[0]);
                toffset[1] += (dim2step[1]);
                toffset[2] += (dim2step[2]);
            }
        }
    } else {
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

        vector<int> toffset(3, 0);
        for (int z = 0; z < (dims[2]/isoblksize); ++z) {
            for (int y = 0; y < (dims[1]/isoblksize); ++y) {
                for (int x = 0; x < (dims[0]/isoblksize); ++x) {
                    toffset[0] = offset[0] + x * isoblksize;
                    toffset[1] = offset[1] + y * isoblksize;
                    toffset[2] = offset[2] + z * isoblksize;
                    libdvid::DVIDCompressedBlock cblock(emptyptr, toffset,
                            isoblksize, bytedepth, compression_type);
                    blocks.push_back(cblock);
                }
            }
        }
    }

    return blocks;
}
