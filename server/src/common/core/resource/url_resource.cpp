#include "file_resource.h"
#include "dataprovider/single_shot_file_dataprovider.h"
#include "dataprovider/streamdataprovider.h"

QnFileResource::QnFileResource(QString filename)
{
    QFileInfo fi(filename);
	setId(fi.absoluteFilePath());
    m_filename = QFileInfo(filename).fileName();
	 
	addDeviceTypeFlag(QnResource::SINGLE_SHOT);

    m_name = "FileResource ";
}

bool QnFileResource::equalsTo(const QnResourcePtr other) const
{
    QnFileResourcePtr r = other.dynamicCast<QnFileResource>();

    if (!r)
        return false;

    return (getFileName() == r->getFileName());
}

QString QnFileResource::getFileName() const
{
	return m_filename;
}

QString QnFileResource::toString() const 
{
	return m_filename;
}

QnStreamDataProvider* QnFileResource::getDeviceStreamConnection()
{
	return new CLSingleShotFileStreamreader(this);
}
