// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QEventLoop>

namespace nx::vms::client::mobile::detail {

/**
 *  Helper class which waits for the qml code execution and then for the specific signal "gotResult"
 *  to retreive the string result of some operation. Locks cretion thread until result is ready:
 *  runs event loop at creation and quits it on result update.
 */
class QmlResultWait: public QObject
{
    Q_OBJECT
public:
    QmlResultWait(QObject* watched, QObject* parent = nullptr);

    QString result() const;

private slots:
    void handleResultUpdated(const QString& value);

private:
    QEventLoop m_loop;
    QString m_result;
};

} // namespace nx::vms::client::mobile::detail
