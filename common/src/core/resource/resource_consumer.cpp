#include "resource_consumer.h"

#include <core/resource/resource.h>

QnResourceConsumer::QnResourceConsumer(const QnResourcePtr& resource):
    m_resource(resource)
{
    if (m_resource)
        m_resource->addConsumer(this);
}

QnResourceConsumer::~QnResourceConsumer()
{
    disconnectFromResource();
}

const QnResourcePtr &QnResourceConsumer::getResource() const
{
    return m_resource;
}

bool QnResourceConsumer::isConnectedToTheResource() const
{
    if (m_resource)
        return m_resource->hasConsumer(const_cast<QnResourceConsumer*>(this));
    else
        return false;
}

void QnResourceConsumer::disconnectFromResource()
{
    if (m_resource)
        m_resource->removeConsumer(this);
}

