#include "resource_consumer.h"


QnResourceConsumer::QnResourceConsumer(QnResourcePtr resource):
    m_resource(resource)
{
    m_resource->addConsumer(this);
}

QnResourceConsumer::~QnResourceConsumer()
{
    disconnectFromResource();
}

QnResourcePtr QnResourceConsumer::getResource() const
{
    return m_resource;
}

bool QnResourceConsumer::isConnectedToTheResource() const
{
    return m_resource->hasSuchConsumer(const_cast<QnResourceConsumer*>(this));
}

void QnResourceConsumer::disconnectFromResource()
{
    if (!isConnectedToTheResource())
        return;

    m_resource->removeConsumer(this);
};
