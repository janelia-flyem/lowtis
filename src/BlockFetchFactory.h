//?! create backend for block fetching


#include <boost/shared_ptr.hpp>
#include <lowtis/LowtisConfig.h>
#include "BlockFetch.h"

namespace lowtis {

BlockFetchPtr create_blockfetcher(LowtisConfig* config);

}
