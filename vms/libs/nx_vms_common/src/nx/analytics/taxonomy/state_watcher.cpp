// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "state_watcher.h"

#include <common/common_module.h>

#include <nx/analytics/taxonomy/descriptor_container.h>
#include <nx/analytics/taxonomy/state_compiler.h>
#include <nx/analytics/taxonomy/state.h>

using namespace nx::vms::api::analytics;

namespace nx::analytics::taxonomy {

StateWatcher::StateWatcher(QnCommonModule* commonModule):
    Connective<AbstractStateWatcher>(commonModule),
    QnCommonModuleAware(commonModule),
    m_state(std::make_shared<State>())
{
    connect(commonModule->descriptorContainer(), &DescriptorContainer::descriptorsUpdated,
        this, &StateWatcher::at_descriptorsUpdated);
}

std::shared_ptr<AbstractState> StateWatcher::state() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (m_state)
        return m_state;

    StateCompiler::Result result = StateCompiler::compile(currentDescriptors());
    m_state = result.state;

    return m_state;
}

Descriptors StateWatcher::currentDescriptors() const
{
    return commonModule()->descriptorContainer()->descriptors();
}

void StateWatcher::at_descriptorsUpdated()
{
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_state.reset();
    }

    emit stateChanged();
}

} // namespace nx::analytics::taxonomy
