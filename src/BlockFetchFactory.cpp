#include <lowtis/BlockFetchFactory.h>
#include <lowtis/DVIDBlockFetch.h>

using namespace lowtis;

BlockFetchPtr lowtis::create_blockfetcher(LowtisConfigPtr config)
{
    LowtisConfig * configraw = config.get();
    
    // create dvid block
    if (auto config = dynamic_cast<DVIDConfig*>(configraw)) {
        return BlockFetchPtr(new DVIDBlockFetch(*(config)));
    }

    return BlockFetchPtr(0);
}
