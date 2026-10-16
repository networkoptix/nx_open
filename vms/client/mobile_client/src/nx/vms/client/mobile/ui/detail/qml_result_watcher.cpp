// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "qml_result_watcher.h"

#include <nx/utils/log/assert.h>

namespace nx::vms::client::mobile::detail {

QmlResultWatcher::QmlResultWatcher(QObject* watched): base_type(watched)
{
    const auto connection = connect(watched,
        SIGNAL(gotResult(const QString&)),
        this,
        SLOT(handleResultUpdated(const QString&)));
    NX_ASSERT(connection, "The watched object has no gotResult(QString) signal");

    // Children are destroyed after the destroyed() signal is emitted.
    connect(watched, &QObject::destroyed, this, [this]() { handleResultUpdated({}); });
}

bool QmlResultWatcher::hasResult() const
{
    return m_hasResult;
}

void QmlResultWatcher::handleResultUpdated(const QString& value)
{
    if (m_hasResult)
        return;

    m_hasResult = true;
    emit resultReady(value);
}

} // namespace nx::vms::client::mobile::detail
