#pragma once

#include <memory>
#include <nx/update/info/file_data.h>
#include <nx/update/info/manual_file_data.h>
#include <nx/update/info/result_code.h>
#include <nx/update/info/update_request_data.h>

namespace nx {
namespace update {
namespace info {

class NX_UPDATE_API AbstractUpdateRegistry
{
public:
    virtual ~AbstractUpdateRegistry() {}

    virtual ResultCode findUpdateFile(
        const UpdateFileRequestData& updateFileRequestData,
        FileData* outFileData) const = 0;

    virtual ResultCode latestUpdate(
        const UpdateRequestData& updateRequestData,
        QnSoftwareVersion* outSoftwareVersion) const = 0;

    virtual void addFileData(const ManualFileData& manualFileData) = 0;
    virtual void removeFileData(const QString& fileName) = 0;
    virtual QList<QString> alternativeServers() const = 0;
    virtual int updateVersion() const = 0;

    virtual QByteArray toByteArray() const = 0;
    virtual bool fromByteArray(const QByteArray& rawData) = 0;
    virtual bool equals(AbstractUpdateRegistry* other) const = 0;
    virtual void merge(AbstractUpdateRegistry* other) = 0;
    virtual QList<QnUuid> additionalPeers(const QString& fileName) const = 0;
};

using AbstractUpdateRegistryPtr = std::unique_ptr<AbstractUpdateRegistry>;

} // namespace info
} // namespace update
} // namespace nx
