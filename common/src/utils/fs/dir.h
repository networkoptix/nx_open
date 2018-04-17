/**********************************************************
* Oct 5, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_DIR_H
#define NX_DIR_H

#include <list>

#include <nx/utils/system_error.h>

struct PartitionInfo
{
    PartitionInfo()
    :
        freeBytes(0),
        sizeBytes(0)
    {
    }

    /** System-dependent name of device */
    QString devName;

    /** Partition's root path. */
    QString path;

    /** Partition's filesystem name. */
    QString fsName;

    /** Free space of this partition in bytes */
    qint64 freeBytes;

    /** Total size of this partition in bytes */
    qint64 sizeBytes;

    bool isUsb = false;
};

class AbstractSystemInfoProvider
{
public:
    virtual ~AbstractSystemInfoProvider() = default;
    virtual QByteArray fileContent() const = 0;
    virtual bool scanfLongPattern() const = 0;
    virtual qint64 totalSpace(const QByteArray& fsPath) const = 0;
    virtual qint64 freeSpace(const QByteArray& fsPath) const = 0;
};

class CommonSystemInfoProvider: public AbstractSystemInfoProvider
{
public:
    CommonSystemInfoProvider();

    virtual QByteArray fileContent() const override;
    virtual bool scanfLongPattern() const override;
    virtual qint64 totalSpace(const QByteArray& fsPath) const override;
    virtual qint64 freeSpace(const QByteArray& fsPath) const override;
    QString fileName() const;

private:
    bool m_scanfLongPattern = false;
    QString m_fileName;
};

SystemError::ErrorCode readPartitions(
    AbstractSystemInfoProvider* systemInfoProvider,
    std::list<PartitionInfo>* const partitionInfoList);

void decodeOctalEncodedPath(char* path);
QString mountsFileName(bool* scanfLongPattern);

#endif  //NX_DIR_H
