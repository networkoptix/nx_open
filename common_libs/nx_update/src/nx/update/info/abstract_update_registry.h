#pragma once

#include <memory>
#include <nx/update/info/file_data.h>
#include <nx/update/info/result_code.h>
#include <nx/update/info/update_request_data.h>

namespace nx {
namespace update {
namespace info {

class NX_UPDATE_API AbstractUpdateRegistry
{
public:
    // todo: implement findClientUpdate()
    virtual ~AbstractUpdateRegistry() {}

    virtual ResultCode findUpdateFile(
        const UpdateFileRequestData& updateFileRequestData,
        FileData* outFileData) const = 0;
    virtual ResultCode latestUpdate(
        const UpdateRequestData& updateRequestData,
        QnSoftwareVersion* outSoftwareVersion) const = 0;
    virtual QList<QString> alternativeServers() const = 0;

    virtual QByteArray toByteArray() const = 0;
    virtual bool fromByteArray(const QByteArray& rawData) = 0;
    virtual bool equals(AbstractUpdateRegistry* other) const = 0;
};

using AbstractUpdateRegistryPtr = std::unique_ptr<AbstractUpdateRegistry>;

} // namespace info
} // namespace update
} // namespace nx
