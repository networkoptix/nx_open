#pragma once

#include <nx/vms/server/fs/partitions/abstract_partitions_information_provider_linux.h>
#include <nx/vms/server/server_module_aware.h>

namespace  nx::vms::server::fs {

class PartitionsInformationProvider:
    public AbstractPartitionsInformationProvider,
    public nx::vms::server::ServerModuleAware
{
  public:
    PartitionsInformationProvider(QnMediaServerModule* serverModule);

    virtual QByteArray mountsFileContent() const override;
    virtual bool isScanfLongPattern() const override;
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

    mutable QMap<QString, DeviceSpaces> m_deviceSpacesCache;
    mutable QnMutex m_mutex;
    mutable int m_tries;
    bool m_scanfLongPattern = false;
    QString m_fileName;
};

} // namespace nx::vms::server::fs
