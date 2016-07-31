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
     * Fetch 3D subvolume from DVID using labelblks.  The size and
     * offset will be adjusted to make the request block aligned.
     * \param dims size of subvolume requestsed
     * \param offset offset of subvolume
     * \return list of compressed blocks
    */
    std::vector<libdvid::DVIDCompressedBlock> extract_blocks(
            std::vector<unsigned int> dims, std::vector<int> offset, int zoom);

    /*!
     * Base class virtual function for retrieving blocks specified.
     * \param blocks loads compressed block data into provided coordinates 
    */
    void extract_specific_blocks(
            std::vector<libdvid::DVIDCompressedBlock>& blocks, int zoom);

    /*!
     * Base class virtual function for finding intersecting blocks.
     * \param dims size of subvolume requestsed
     * \param offset offset of subvolume
     * \return list of compressed blocks
    */
    std::vector<libdvid::DVIDCompressedBlock> intersecting_blocks(
            std::vector<unsigned int> dims, std::vector<int> offset);


  private:
    std::string labeltypename;
    libdvid::DVIDNodeService node_service;
    size_t blocksize;
};

}

#endif
