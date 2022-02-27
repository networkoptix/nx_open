// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtQml/QJSValue>

class QQuickItem;

namespace nx::vms::client::desktop {

/**
 * A more convenient replacement for QQuickItem::grabToImage - also with hi-dpi bugs fixed.
 *
 * Callback is different from QQuickItem::grabToImage: it has just one string url argument.
 */
class ItemGrabber: public QObject
{
    Q_OBJECT

public:
    Q_INVOKABLE static void grabToImage(QQuickItem* item, const QJSValue& callback);

    static void registerQmlType();
};

} // namespace nx::vms::client::desktop
