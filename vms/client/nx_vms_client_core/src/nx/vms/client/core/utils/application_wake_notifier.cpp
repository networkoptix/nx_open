// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "application_wake_notifier.h"

#include <QtGui/QGuiApplication>

#include <nx/utils/log/log.h>

namespace nx::vms::client::core {

ApplicationWakeNotifier::ApplicationWakeNotifier(QObject* parent):
    QObject(parent),
    m_wasSuspended(qApp->applicationState() == Qt::ApplicationSuspended)
{
    connect(qApp,
        &QGuiApplication::applicationStateChanged,
        this,
        [this](Qt::ApplicationState state)
        {
            if (state == Qt::ApplicationSuspended)
            {
                NX_DEBUG(this, "Application is suspended");
                m_wasSuspended = true;
            }
            else if (state == Qt::ApplicationActive && m_wasSuspended)
            {
                NX_DEBUG(this, "Application woke up");
                m_wasSuspended = false;
                emit wokeUp();
            }
        });
}

bool ApplicationWakeNotifier::isInBackground() const
{
    return qApp->applicationState() != Qt::ApplicationActive;
}

} // namespace nx::vms::client::core
