// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

namespace nx::vms::client::mobile::detail {

/**
 * Asynchronous counterpart of QmlResultWait. Watches the specific signal "gotResult" of the qml
 * object and emits resultReady once: with the received result, or with an empty one if the object
 * is destroyed without a result. Is owned by the watched object.
 */
class QmlResultWatcher: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit QmlResultWatcher(QObject* watched);

    bool hasResult() const;

signals:
    void resultReady(const QString& result);

private slots:
    void handleResultUpdated(const QString& value);

private:
    bool m_hasResult = false;
};

} // namespace nx::vms::client::mobile::detail
