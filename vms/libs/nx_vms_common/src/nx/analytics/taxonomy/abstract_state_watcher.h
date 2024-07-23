// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/analytics/taxonomy/abstract_state.h>

namespace nx::analytics::taxonomy {

class NX_VMS_COMMON_API AbstractStateWatcher: public QObject
{
    Q_OBJECT

public:
    AbstractStateWatcher(QObject* parent = nullptr): QObject(parent) {}

    virtual ~AbstractStateWatcher() {}

    virtual std::shared_ptr<AbstractState> state() const = 0;

signals:
    void stateChanged();
};

} // namespace nx::analytics::taxonomy
