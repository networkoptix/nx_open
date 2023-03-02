// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "state_watcher.h"

#include <common/common_module.h>

#include <nx/analytics/taxonomy/descriptor_container.h>
#include <nx/analytics/taxonomy/state_compiler.h>
#include <nx/analytics/taxonomy/state.h>

using namespace nx::vms::api::analytics;

namespace nx::analytics::taxonomy {

StateWatcher::StateWatcher(
    DescriptorContainer* taxonomyDescriptorContainer,
    QObject* parent)
    :
    AbstractStateWatcher(parent),
    m_taxonomyDescriptorContainer(taxonomyDescriptorContainer),
    m_state(std::make_shared<State>())
{
    connect(m_taxonomyDescriptorContainer, &DescriptorContainer::descriptorsUpdated,
        this, &StateWatcher::at_descriptorsUpdated);
}

void StateWatcher::beforeUpdate()
{
    m_taxonomyDescriptorContainer->disconnect(this);
}

void StateWatcher::afterUpdate()
{
    connect(m_taxonomyDescriptorContainer,
        &DescriptorContainer::descriptorsUpdated,
        this,
        &StateWatcher::at_descriptorsUpdated);
    at_descriptorsUpdated();
}

std::shared_ptr<AbstractState> StateWatcher::state() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_state;
}

Descriptors StateWatcher::currentDescriptors() const
{
    return m_taxonomyDescriptorContainer->descriptors();
}

void StateWatcher::at_descriptorsUpdated()
{
    StateCompiler::Result result = StateCompiler::compile(currentDescriptors());

    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_state = result.state;
    }

    emit stateChanged();
}

} // namespace nx::analytics::taxonomy
