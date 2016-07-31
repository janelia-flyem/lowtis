#ifndef BLOCKFETCH_H
#define BLOCKFETCH_H

#include <memory>
#include <libdvid/DVIDBlocks.h>

// ?! base class for fetching blocks (derived types: libdvid blocks and moc server)
// ?! create once and keep fetching -- reuse dvid node

namespace lowtis {

// TODO: use more generic 'block' type for slices and cubes
// always assume a rectangular cuboid

class BlockFetch {
  public:
    /*!
     * Base class virtual function for retrieving subvolume as blocks.
     * \param dims size of subvolume requestsed
     * \param offset offset of subvolume
     * \param zoom level for downsampled image
     * \return list of compressed blocks
    */
    virtual std::vector<libdvid::DVIDCompressedBlock> extract_blocks(
            std::vector<unsigned int> dims, std::vector<int> offset, int zoom) = 0;

    /*!
     * Base class virtual function for retrieving blocks specified.
     * \param blocks loads compressed block data into provided coordinates 
    */
    virtual void extract_specific_blocks(
            std::vector<libdvid::DVIDCompressedBlock>& blocks, int zoom) = 0;

    /*!
     * Base class virtual function for finding intersecting blocks.
     * \param dims size of subvolume requestsed
     * \param offset offset of subvolume
     * \param zoom level for downsampled image
     * \return list of compressed blocks
    */
    virtual std::vector<libdvid::DVIDCompressedBlock> intersecting_blocks(
            std::vector<unsigned int> dims, std::vector<int> offset) = 0;


};

typedef std::shared_ptr<BlockFetch> BlockFetchPtr;

}

#endif
