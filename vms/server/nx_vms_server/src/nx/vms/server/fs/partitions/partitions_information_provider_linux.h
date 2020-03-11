#pragma once

#include <nx/vms/server/fs/partitions/abstract_partitions_information_provider_linux.h>
#include <nx/utils/thread/mutex.h>

class QnGlobalSettings;
namespace nx::vms::server { class RootFileSystem; }

namespace  nx::vms::server::fs {

class PartitionsInformationProvider: public AbstractPartitionsInformationProvider
{
  public:
    PartitionsInformationProvider(QnGlobalSettings* globalSettings, RootFileSystem* rootFs);

    virtual QByteArray mountsFileContents() const override;
    virtual qint64 totalSpace(const QByteArray& fsPath) const override;
    virtual qint64 freeSpace(const QByteArray& fsPath) const override;
    virtual bool isFolder(const QByteArray& fsPath) const override;
    virtual QStringList additionalLocalFsTypes() const override;

  private:
    static const qint64 kUnknownValue = std::numeric_limits<qint64>::min();
    struct DeviceSpaces
    {
        qint64 freeSpace = kUnknownValue;
        qint64 totalSpace = kUnknownValue;
    };

    QnGlobalSettings* m_globalSettings = nullptr;
    nx::vms::server::RootFileSystem* m_rootFs = nullptr;
    mutable QMap<QString, DeviceSpaces> m_deviceSpacesCache;
    mutable QnMutex m_mutex;
    mutable int m_tries;
    QString m_fileName;
    mutable QnMutex m_fsTypesMutex;
    mutable QString m_additionalFsTypesString;
    mutable QStringList m_additionalFsTypes;
};

} // namespace nx::vms::server::fs
