// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_stream_data_provider.h"

#include <core/resource/resource.h>
#include <core/dataconsumer/abstract_data_receptor.h>
#include <nx/utils/log/log.h>

QnAbstractStreamDataProvider::QnAbstractStreamDataProvider(const QnResourcePtr& resource):
    m_resource(resource),
    m_role(Qn::CR_Default)
{
    NX_DEBUG(this, "New");
}

QnAbstractStreamDataProvider::~QnAbstractStreamDataProvider()
{
    NX_DEBUG(this, "Delete");
    stop();
}

bool QnAbstractStreamDataProvider::dataCanBeAccepted() const
{
    // need to read only if all queues has more space and at least one queue is exist
    auto dataProcessors = m_dataprocessors.lock();
    for (int i = 0; i < dataProcessors->size(); ++i)
    {
        QnAbstractMediaDataReceptor* dp = dataProcessors->at(i);
        if (!dp->canAcceptData())
            return false;
    }

    return true;
}

int QnAbstractStreamDataProvider::processorsCount() const
{
    return m_dataprocessors.lock()->size();
}

void QnAbstractStreamDataProvider::addDataProcessor(QnAbstractMediaDataReceptor* dp)
{
    NX_CRITICAL(dp);
    auto dataProcessors = m_dataprocessors.lock();
    if (!dataProcessors->contains(dp))
    {
        NX_DEBUG(this, "Add data processor: %1", dp);
        dataProcessors->push_back(dp);
        dp->consumers += 1;
    }
}

bool QnAbstractStreamDataProvider::needConfigureProvider() const
{
    const auto dataProviders = m_dataprocessors.lock();
    for (auto processor: *dataProviders)
    {
        if (processor->needConfigureProvider())
            return true;
    }
    return false;
}

void QnAbstractStreamDataProvider::removeDataProcessor(QnAbstractMediaDataReceptor* dp)
{
    if (!dp)
        return;

    if (m_dataprocessors.lock()->removeOne(dp))
    {
        dp->consumers -= 1;
        NX_DEBUG(this, "Remove data processor: %1", dp);
    }
    else
    {
        NX_DEBUG(this, "Remove not added data processor: %1", dp);
    }
}

void QnAbstractStreamDataProvider::putData(const QnAbstractDataPacketPtr& data)
{
    if (!data)
        return;

    const auto dataProcessors = m_dataprocessors.lock();
    for (const auto& p: *dataProcessors)
        p->putData(data);
}

void QnAbstractStreamDataProvider::setRole(Qn::ConnectionRole role)
{
    NX_ASSERT(!isRunning());
    m_role = role;
}

Qn::ConnectionRole QnAbstractStreamDataProvider::getRole() const
{
    return m_role;
}

const QnResourcePtr& QnAbstractStreamDataProvider::getResource() const
{
    return m_resource;
}
