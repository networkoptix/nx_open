#include "resource_consumer.h"

#include <core/resource/resource.h>

QnResourceConsumer::QnResourceConsumer(const QnResourcePtr& resource):
    m_resource(resource)
{
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
    return m_resource->hasConsumer(const_cast<QnResourceConsumer*>(this));
}

void QnResourceConsumer::disconnectFromResource()
{
    m_resource->removeConsumer(this);
}

