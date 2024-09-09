// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/analytics/taxonomy/abstract_state_watcher.h>
#include <nx/utils/thread/mutex.h>
#include <nx/vms/common/system_context_aware.h>

namespace nx::analytics::taxonomy {

class State;
class DescriptorContainer;

class StateWatcher: public AbstractStateWatcher, public nx::vms::common::SystemContextAware
{
public:
    StateWatcher(
        DescriptorContainer* taxonomyDescriptorContainer,
        nx::vms::common::SystemContext* systemContext,
        QObject* parent = nullptr);

    virtual std::shared_ptr<AbstractState> state() const override;

private:
    nx::vms::api::analytics::Descriptors currentDescriptors() const;

    void at_descriptorsUpdated();

private:
    DescriptorContainer* const m_taxonomyDescriptorContainer;
    mutable nx::Mutex m_mutex;
    mutable std::shared_ptr<AbstractState> m_state;
};

} // namespace nx::analytics::taxonomy
