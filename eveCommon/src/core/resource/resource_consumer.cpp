#include "resource_consumer.h"


QnResourceConsumer::QnResourceConsumer(QnResource* resource):
m_resource(resource)
{
    m_resource->addConsumer(this);
}

QnResourceConsumer::~QnResourceConsumer()
{
    disconnectFromResource();
}

QnResource* QnResourceConsumer::getResource() const
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
