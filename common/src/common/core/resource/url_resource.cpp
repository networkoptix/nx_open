#include "dataprovider/single_shot_file_dataprovider.h"
#include "url_resource.h"


QnURLResource::QnURLResource(QString filename)
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

QString QnURLResource::getUrl() const
{
    QMutexLocker locker(&m_mutex);
	return m_url;
}

QString QnURLResource::toString() const 
{
	return m_url;
}

