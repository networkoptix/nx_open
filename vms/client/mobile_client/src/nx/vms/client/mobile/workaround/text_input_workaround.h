// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

class QQuickItem;

namespace nx::vms::client::mobile {

/**
 * Workaround for QTBUG-55636/QTBUG-80790. Prevent window scrolling up on keyboard opening in iOS.
 * The idea is to install an event filter on QQuickItem and listen for a QEvent::InputMethodQuery
 * with Qt::InputMethodQuery::ImCursorRectangle. Then we set its value to empty QRectF and Qt will
 * no longer scroll the view to show that text field.
 */
class TextInputWorkaround: public QObject
{
    Q_OBJECT

public:
    static void registerQmlType();

    Q_INVOKABLE void setup(QQuickItem* item) const;
};

} // namespace nx::vms::client::mobile
