#include "meta_types.h"

#include <nx/fusion/model_functions.h>

#include <plugins/resource/hanwha/hanwha_common.h>

namespace nx::vms::server {

void registerSerializers()
{
    QnJsonSerializer::registerSerializer<
        nx::vms::server::plugins::HanwhaBypassSupportType>();
}

} // namespace nx::mediaserver
