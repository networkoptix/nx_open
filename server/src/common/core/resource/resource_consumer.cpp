#include "resource_consumer.h"


QnResourceConsumer::QnResourceConsumer(QnResourcePtr resource):
m_resource(resource)
{

}

QnResourceConsumer::~QnResourceConsumer()
{

}

QnResourcePtr QnResourceConsumer::getResource() const
{
    return m_resource;
}

void QnResourceConsumer::isConnectedToTheResource() const
{
    return m_resource->hasSuchConsumer(this);
}

virtual void QnResourceConsumer::beforeDisconnectFromResource()
{

}

virtual void QnResourceConsumer::disconnectFromResource()
{
    if (!isConnectedToTheResource())
        return;

    m_resource->removeConsumer(this);
};
