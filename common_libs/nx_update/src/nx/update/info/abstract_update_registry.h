#pragma once

#include <nx/update/info/file_data.h>
#include <nx/update/info/result_code.h>
#include <nx/update/info/update_request_data.h>

namespace nx {
namespace update {
namespace info {

class NX_UPDATE_API AbstractUpdateRegistry
{
public:
    // todo: serialize and deserialize
    virtual ~AbstractUpdateRegistry() {}
    virtual ResultCode findUpdate(
        const UpdateRequestData& updateRequestData,
        FileData* outFileData) = 0;
    virtual QList<QString> alternativeServers() const = 0;
};

} // namespace info
} // namespace update
} // namespace nx
