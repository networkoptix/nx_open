// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/analytics/taxonomy/abstract_state_watcher.h>

#include <nx/utils/thread/mutex.h>
#include <common/common_module_aware.h>
#include <utils/common/connective.h>

namespace nx::analytics::taxonomy {

class State;

class StateWatcher: public Connective<AbstractStateWatcher>, public QnCommonModuleAware
{
    Q_OBJECT
public:
    StateWatcher(QnCommonModule* commonModule);

    virtual std::shared_ptr<AbstractState> state() const override;

private:
    nx::vms::api::analytics::Descriptors currentDescriptors() const;

    void at_descriptorsUpdated();

private:
    mutable nx::Mutex m_mutex;
    mutable std::shared_ptr<AbstractState> m_state;
};

} // namespace nx::analytics::taxonomy
