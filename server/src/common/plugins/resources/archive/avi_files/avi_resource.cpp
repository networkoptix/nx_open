#include "avi_resource.h"
#include "avi_strem_dataprovider.h"

QnAviResource::QnAviResource(const QString& file)
{
    QFileInfo fi(file);

	m_url = fi.absoluteFilePath();
	m_name = QFileInfo(file).fileName();
}

QnAviResource::~QnAviResource()
{

}

bool QnAviResource::equalsTo(const QnResourcePtr other) const
{
    QnAviResourcePtr r = other.dynamicCast<QnAviResource>();

    if (!r)
        return false;

    return (getUrl() == r->getUrl());
}

QString QnAviResource::getUrl() const
{
    return m_url;
}

QString QnAviResource::toString() const
{
	return m_name;
}

QnMediaStreamDataProvider* QnAviResource::getDeviceStreamConnection()
{
	return new QnPlAVIStreamProvider(this);
}

