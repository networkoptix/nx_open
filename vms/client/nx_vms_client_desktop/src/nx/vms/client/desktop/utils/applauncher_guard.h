// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

class QTimerEvent;

namespace nx::vms::client::desktop {

/** This class keeps applaucher running. */
class ApplauncherGuard: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    ApplauncherGuard(QObject* parent = nullptr);

protected:
    virtual void timerEvent(QTimerEvent* event) override;
};

} // namespace nx::vms::client::desktop
