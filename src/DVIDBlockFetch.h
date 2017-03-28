#ifndef DVIDBLOCKFETCH_H
#define DVIDBLOCKFETCH_H

#include "BlockFetch.h"
#include <lowtis/LowtisConfig.h>
#include <libdvid/DVIDNodeService.h>

namespace lowtis {

class DVIDBlockFetch : public BlockFetch {
  public:
    /*! Create DVIDNodeService for this datatype name.
     * TODO: Support grayscale blocks as well.
     * \param config contains server, uuid, user, datatypename
    */
    DVIDBlockFetch(DVIDConfig& config);

    /*!
     * Non-blocking prefetch on specified blocks.  Only supported
     * for uint8blk data and does server-side prefetching (data
     * will not be local) and should be used generally when
     * the underlying DB gives poor latency.
    */
    void prefetch_blocks(std::vector<libdvid::DVIDCompressedBlock>& blocks, int zoom);

    /*!
     * Base class virtual function for retrieving blocks specified.
     * \param blocks loads compressed block data into provided coordinates 
    */
    void extract_specific_blocks(
            std::vector<libdvid::DVIDCompressedBlock>& blocks, int zoom);

  private:

    /*!
     * Fetch 3D subvolume from DVID using labelblks.  The size and
     * offset will be adjusted to make the request block aligned.
     * \param dims size of subvolume requestsed
     * \param offset offset of subvolume
     * \return list of compressed blocks
    */
    std::vector<libdvid::DVIDCompressedBlock> extract_blocks(
            std::vector<unsigned int> dims, std::vector<int> offset, int zoom);


    std::string labeltypename;
    bool usehighiopquery;
    libdvid::DVIDNodeService node_service;
};

}

#endif
