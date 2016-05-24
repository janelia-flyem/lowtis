#include "BlockFetchFactory.h"
#include "DVIDBlockFetch.h"

using namespace lowtis;

BlockFetchPtr lowtis::create_blockfetcher(LowtisConfig* config)
{
    
    // create dvid block
    if (auto configcast = dynamic_cast<DVIDLabelblkConfig*>(config)) {
        return BlockFetchPtr(new DVIDBlockFetch(*(configcast)));
    }

    return BlockFetchPtr(0);
}
