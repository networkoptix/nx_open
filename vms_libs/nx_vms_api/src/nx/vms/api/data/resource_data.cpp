#include "resource_data.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace api {

QnUuid ResourceData::getFixedTypeId(const QString& typeName)
{
    return QnUuid::fromArbitraryData(typeName.toUtf8() + QByteArray("-"));
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ResourceParamData)(ResourceParamWithRefData)(ResourceData)(ResourceStatusData),
    (eq)(ubjson)(xml)(json)(sql_record)(csv_record),
    _Fields)

} // namespace api
} // namespace vms
} // namespace nx
