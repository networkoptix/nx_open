// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "composite_resource_source.h"

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

CompositeResourceSource::CompositeResourceSource(
    AbstractResourceSourcePtr firstSource,
    AbstractResourceSourcePtr secondSource)
    :
    base_type(),
    m_firstSource(std::move(firstSource)),
    m_secondSource(std::move(secondSource))
{
    const auto firstSourceResources = m_firstSource->getResources();
    m_firstSourceResources =
        QSet<QnResourcePtr>(firstSourceResources.cbegin(), firstSourceResources.cend());

	const auto secondSourceResources = m_secondSource->getResources();
    m_secondSourceResources =
        QSet<QnResourcePtr>(secondSourceResources.cbegin(), secondSourceResources.cend());

    connect(m_firstSource.get(), &AbstractResourceSource::resourceAdded, this,
        [this](const QnResourcePtr& resource)
        {
            m_firstSourceResources.insert(resource);
            if (!m_secondSourceResources.contains(resource))
                emit resourceAdded(resource);
        });

    connect(m_firstSource.get(), &AbstractResourceSource::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            m_firstSourceResources.remove(resource);
            if (!m_secondSourceResources.contains(resource))
                emit resourceRemoved(resource);
        });

    connect(m_secondSource.get(), &AbstractResourceSource::resourceAdded, this,
        [this](const QnResourcePtr& resource)
        {
            m_secondSourceResources.insert(resource);
            if (!m_firstSourceResources.contains(resource))
                emit resourceAdded(resource);
        });

    connect(m_secondSource.get(), &AbstractResourceSource::resourceRemoved, this,
        [this](const QnResourcePtr& resource)
        {
            m_secondSourceResources.remove(resource);
		    if (!m_firstSourceResources.contains(resource))
                emit resourceRemoved(resource);
        });
}

QVector<QnResourcePtr> CompositeResourceSource::getResources()
{
    const auto unitedResources = m_firstSourceResources | m_secondSourceResources;
    return QVector<QnResourcePtr>(unitedResources.cbegin(), unitedResources.cend());
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
