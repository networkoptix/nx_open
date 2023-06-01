// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/analytics/taxonomy/abstract_state_watcher.h>
#include <nx/vms/common/system_context_aware.h>
#include <nx/utils/thread/mutex.h>

namespace nx::analytics::taxonomy {

class State;
class DescriptorContainer;

class StateWatcher: public AbstractStateWatcher, public nx::vms::common::SystemContextAware
{
    Q_OBJECT
public:
    StateWatcher(
        DescriptorContainer* taxonomyDescriptorContainer,
        nx::vms::common::SystemContext* systemContext,
        QObject* parent = nullptr);

    virtual std::shared_ptr<AbstractState> state() const override;

protected:
    /** Called in first beginUpdate(). isUpdating is false. */
    virtual void beforeUpdate() override;

    /** Called in last endUpdate(). isUpdating is false. */
    virtual void afterUpdate() override;

private:
    nx::vms::api::analytics::Descriptors currentDescriptors() const;

    void at_descriptorsUpdated();

private:
    DescriptorContainer* const m_taxonomyDescriptorContainer;
    mutable nx::Mutex m_mutex;
    mutable std::shared_ptr<AbstractState> m_state;
};

} // namespace nx::analytics::taxonomy
