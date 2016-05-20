#include <lowtis/BlockFetchFactory.h>
#include <lowtis/DVIDBlockFetch.h>

using namespace lowtis;

BlockFetchPtr lowtis::create_blockfetcher(LowtisConfig* config)
{
    
    // create dvid block
    if (auto configcast = dynamic_cast<DVIDConfig*>(config)) {
        return BlockFetchPtr(new DVIDBlockFetch(*(configcast)));
    }

    return BlockFetchPtr(0);
}
