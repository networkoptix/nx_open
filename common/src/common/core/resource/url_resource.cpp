#include "dataprovider/single_shot_file_dataprovider.h"
#include "url_resource.h"

QnURLResource::QnURLResource()
{
    addFlag(QnResource::url);
}

QnURLResource::QnURLResource(const QString& url): m_url(url)
{
    addFlag(QnResource::url);
}

bool QnURLResource::equalsTo(const QnResourcePtr other) const
{
    QnUrlResourcePtr r = other.dynamicCast<QnURLResource>();

    if (!r)
        return false;

    return (getUrl() == r->getUrl());
}

void QnURLResource::setUrl(const QString& url) 
{
    m_url = url;
}

QString QnURLResource::getUrl() const
{
    QMutexLocker locker(&m_mutex);
	return m_url;
}

QString QnURLResource::toString() const 
{
	return m_url;
}

