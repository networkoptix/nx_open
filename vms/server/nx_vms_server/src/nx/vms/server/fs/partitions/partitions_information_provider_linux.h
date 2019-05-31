#pragma once

#include <nx/vms/server/fs/partitions/abstract_partitions_information_provider_linux.h>
#include <nx/vms/server/root_fs.h>

namespace  nx::vms::server::fs {

class PartitionsInformationProvider: public AbstractPartitionsInformationProvider
{
  public:
    PartitionsInformationProvider(RootFileSystem* rootFs);

    virtual QByteArray mountsFileContent() const override;
    virtual qint64 totalSpace(const QByteArray& fsPath) const override;
    virtual qint64 freeSpace(const QByteArray& fsPath) const override;
    virtual bool isFolder(const QByteArray& fsPath) const override;

  private:
    static const qint64 kUnknownValue = std::numeric_limits<qint64>::min();
    struct DeviceSpaces
    {
        qint64 freeSpace = kUnknownValue;
        qint64 totalSpace = kUnknownValue;
    };

    RootFileSystem* m_rootFs = nullptr;
    mutable QMap<QString, DeviceSpaces> m_deviceSpacesCache;
    mutable QnMutex m_mutex;
    mutable int m_tries;
    QString m_fileName;
};

} // namespace nx::vms::server::fs
