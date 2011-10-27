#include <QFileInfo>

#include "file_resource.h"

#include "../dataprovider/single_shot_file_reader.h"

QnLocalFileResource::QnLocalFileResource(const QString &filename)
{
    QFileInfo fi(filename);
    setName(fi.fileName());
    m_fileName = fi.absoluteFilePath();
    addFlag(QnResource::SINGLE_SHOT);
}

QnAbstractStreamDataProvider* QnLocalFileResource::createDataProvider(ConnectionRole role)
{
    return new CLSingleShotFileStreamreader(this);
}

QString QnLocalFileResource::getUniqueId() const
{
    return m_fileName;
}
