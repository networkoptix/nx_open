#pragma once

#include <QtCore/QByteArray>

namespace nx::vms::server::fs {

class AbstractPartitionsInformationProvider
{
  public:
    virtual ~AbstractPartitionsInformationProvider() = default;

    virtual QByteArray mountsFileContent() const = 0;
    virtual bool isScanfLongPattern() const = 0;
    virtual qint64 totalSpace(const QByteArray& fsPath) const = 0;
    virtual qint64 freeSpace(const QByteArray& fsPath) const = 0;
    virtual bool isFolder(const QByteArray& fsPath) const = 0;
};

} // namespace nx::vms::server::fs
