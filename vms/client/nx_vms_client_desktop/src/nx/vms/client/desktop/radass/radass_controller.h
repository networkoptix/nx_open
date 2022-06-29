// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>


#include "radass_fwd.h"
#include "utils/abstract_timers.h"

namespace nx::vms::client::desktop {

class AbstractVideoDisplay;

class NX_VMS_CLIENT_DESKTOP_API RadassController: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit RadassController(QObject* parent = nullptr);
    // Constructor for making RAD ASS tests with dummy timers.
    explicit RadassController(TimerFactoryPtr timerFactory, QObject* parent = nullptr);
    virtual ~RadassController() override;

    void registerConsumer(AbstractVideoDisplay* display);
    void unregisterConsumer(AbstractVideoDisplay* display);
    int consumerCount() const;

    RadassMode mode(AbstractVideoDisplay* display) const;
    void setMode(AbstractVideoDisplay* display, RadassMode mode);

signals:
    void performanceCanBeImproved();

public:
    /** Inform controller that not enough data or CPU for stream */
    void onSlowStream(AbstractVideoDisplay* display);

    /** Inform controller that no more problem with stream */
    void streamBackToNormal(AbstractVideoDisplay* display);

private:
    void onTimer();

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
