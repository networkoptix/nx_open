#include "meta_types.h"

#include <nx/fusion/model_functions.h>

#include <plugins/resource/hanwha/hanwha_common.h>

namespace nx::mediaserver {

void registerSerializers()
{
    QnJsonSerializer::registerSerializer<
        nx::mediaserver_core::plugins::HanwhaBypassSupportType>();
}

} // namespace nx::mediaserver
