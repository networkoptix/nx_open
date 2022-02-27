// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
    return m_resource && m_resource->hasConsumer(const_cast<QnResourceConsumer*>(this));
}

void QnResourceConsumer::disconnectFromResource()
{
    if (m_resource)
        m_resource->removeConsumer(this);
}

