#include <QFileInfo>

#include "file_resource.h"
#include "core/dataprovider/single_shot_file_dataprovider.h"


QnLocalFileResource::QnLocalFileResource(const QString &filename)
{
    QFileInfo fi(filename);
    setName(fi.fileName());
    m_fileName = fi.absoluteFilePath();
    addFlag(QnResource::SINGLE_SHOT);
}

QnAbstractStreamDataProvider* QnLocalFileResource::createDataProviderInternal(ConnectionRole role)
{
    return new CLSingleShotFileStreamreader(toSharedPointer());
}

QString QnLocalFileResource::getUniqueId() const
{
    return m_fileName;
}
