#ifndef BLOCKFETCH_H
#define BLOCKFETCH_H

#include <memory>
#include <libdvid/DVIDBlocks.h>

// ?! base class for fetching blocks (derived types: libdvid blocks and moc server)
// ?! create once and keep fetching -- reuse dvid node

namespace lowtis {

// Not thread safe!
// TODO: use more generic 'block' type for slices and cubes
// always assume a rectangular cuboid.
class BlockFetch {
  public:
    /*!
     * Base class virtual function for retrieving blocks specified.
     * \param blocks loads compressed block data into provided coordinates 
    */
    virtual void extract_specific_blocks(
            std::vector<libdvid::DVIDCompressedBlock>& blocks, int zoom) = 0;

    /*!
     * Base class to do non-blocking prefetch on specified blocks.
     * If a BlockFetch derived object does not support prefetching it will
     * just be a no-op.
    */
    virtual void prefetch_blocks(std::vector<libdvid::DVIDCompressedBlock>& blocks, int zoom)
    {
        return;
    }

    /*!
     * Finds intersecting blocks.
     * TODO: supports only isotropic blocks now
     * \param dims size of subvolume requestsed
     * \param offset offset of subvolume
     * \param zoom level for downsampled image
     * \return list of compressed blocks
    */
    std::vector<libdvid::DVIDCompressedBlock> intersecting_blocks(
            std::vector<unsigned int> dims, std::vector<int> offset);

  protected:
    size_t bytedepth;
    std::tuple<size_t, size_t, size_t> blocksize;
    libdvid::DVIDCompressedBlock::CompressType compression_type;
};

typedef std::shared_ptr<BlockFetch> BlockFetchPtr;

}

#endif
