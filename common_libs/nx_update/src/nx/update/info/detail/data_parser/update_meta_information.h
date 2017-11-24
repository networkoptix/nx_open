#pragma once

#include <nx/update/info/detail/data_parser/customization_information.h>
#include <nx/update/info/detail/data_parser/alternative_server_information.h>

namespace nx {
namespace update {
namespace info {
namespace detail {
namespace data_parser {

struct UpdateMetaInformation
{
    QList<CustomizationInformation> customizationInformationList;
    QList<AlternativeServerInformation> alternativeServerInformationList;
};

} // namespace data_parser
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
