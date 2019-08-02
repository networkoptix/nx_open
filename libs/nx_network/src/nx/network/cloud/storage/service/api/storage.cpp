#include "storage.h"

#include <nx/fusion/model_functions.h>

namespace nx::cloud::storage::service::api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (Device)(Storage),
    (ubjson)(json),
    _Fields)

bool Device::operator==(const Device& other) const
{
    return type == other.type
        && dataUrl == other.dataUrl;
}

bool Storage::operator==(const Storage& other) const
{
    return id == other.id
        && region == other.region
        && totalSpace == other.totalSpace
        && freeSpace == other.freeSpace
        && ioDevice == other.ioDevice;
}

} // namespace nx::cloud::storage::service::api