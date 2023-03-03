// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/analytics/taxonomy/abstract_state.h>
#include <utils/common/updatable.h>

namespace nx::analytics::taxonomy {

class NX_VMS_COMMON_API AbstractStateWatcher: public QObject, public QnUpdatable
{
    Q_OBJECT

public:
    AbstractStateWatcher(QObject* parent): QObject(parent) {}

    virtual ~AbstractStateWatcher() {}

    virtual std::shared_ptr<AbstractState> state() const = 0;

signals:
    void stateChanged();
};

} // namespace nx::analytics::taxonomy
