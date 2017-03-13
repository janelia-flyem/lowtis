#include "BlockFetchFactory.h"
#include "DVIDBlockFetch.h"
#include "GoogleBlockFetch.h"

using namespace lowtis;

BlockFetchPtr lowtis::create_blockfetcher(LowtisConfig* config)
{
    // create dvid block
    if (auto configcast = dynamic_cast<DVIDLabelblkConfig*>(config)) {
        return BlockFetchPtr(new DVIDBlockFetch(*(configcast)));
    } else if (auto configcast = dynamic_cast<DVIDGrayblkConfig*>(config)) {
        return BlockFetchPtr(new DVIDBlockFetch(*(configcast)));
    } else if (auto configcast = dynamic_cast<GoogleGrayblkConfig*>(config)) {
        return BlockFetchPtr(new GoogleBlockFetch(*(configcast)));
    }

    return BlockFetchPtr(0);
}
