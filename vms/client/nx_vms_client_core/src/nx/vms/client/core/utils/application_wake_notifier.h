// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

namespace nx::vms::client::core {

/**
 * Reports that the application became active again after having been suspended.
 *
 * A suspended application keeps running for a while and is then frozen, so when it comes back its
 * timers no longer reflect real time. Classes that poll therefore need a nudge on wake-up instead
 * of waiting for a schedule that may be minutes away.
 *
 * Only iOS and Android report Qt::ApplicationSuspended, so on desktop this never emits.
 */
class NX_VMS_CLIENT_CORE_API ApplicationWakeNotifier: public QObject
{
    Q_OBJECT

public:
    explicit ApplicationWakeNotifier(QObject* parent = nullptr);

    /** Whether the application is in the background, i.e. the user is not looking at it. */
    bool isInBackground() const;

signals:
    void wokeUp();

private:
    // applicationStateChanged() reports only the current state. Remember suspension to distinguish
    // wake-up from a focus regain.
    bool m_wasSuspended = false;
};

} // namespace nx::vms::client::core
