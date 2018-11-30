#include "meta_types.h"

#include <nx/fusion/model_functions.h>

#include <nx/mediaserver_core/plugins/resource_data_support/hanwha.h>

namespace nx::mediaserver {

void registerSerializers()
{
    QnJsonSerializer::registerSerializer<
        nx::mediaserver_core::plugins::resource_data_support::HanwhaBypassSupportType>();
}

} // namespace nx::mediaserver
