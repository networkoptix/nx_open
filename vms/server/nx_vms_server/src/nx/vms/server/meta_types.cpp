#include "meta_types.h"

#include <nx/fusion/model_functions.h>

#include <nx/vms/server/plugins/resource_data_support/hanwha.h>

#include <core/onvif/onvif_config_data.h>

namespace nx::vms::server {

void registerSerializers()
{
    QnJsonSerializer::registerSerializer<
        nx::vms::server::plugins::resource_data_support::HanwhaBypassSupportType>();

    QnJsonSerializer::registerSerializer<QnOnvifConfigDataPtr>();
}

} // namespace nx::vms::server
