// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "qml_result_wait.h"

namespace nx::vms::client::mobile::detail {

QmlResultWait::QmlResultWait(QObject* watched, QObject* parent):
    QObject(parent)
{
    connect(watched, SIGNAL(gotResult(const QString&)),
        this, SLOT(handleResultUpdated(const QString&)));

    connect(watched, &QObject::destroyed, &m_loop, &QEventLoop::quit);
    m_loop.exec();
}

QString QmlResultWait::result() const
{
    return m_result;
}

void QmlResultWait::handleResultUpdated(const QString& value)
{
    m_result = value;
    m_loop.quit();
}

} // namespace nx::vms::client::mobile::detail
