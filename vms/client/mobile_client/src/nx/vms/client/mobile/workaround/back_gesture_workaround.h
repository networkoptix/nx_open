// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtQuick/QQuickItem>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::mobile {

/**
 * When back gesture in Android 10 is happened QML MouseArea hangs because of the bug
 * in the implementation. This is C++ part of workaround.
 * TODO: #ynikitenkov Check if the newest version of Qt fixes it.
 */
class BackGestureWorkaround: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_PROPERTY(QQuickItem* workaroundParentItem
        READ workaroundParentItem
        WRITE setWorkaroundParentItem
        NOTIFY workaroundParentItemChanged)

public:
    BackGestureWorkaround(QObject* parent = nullptr);
    virtual ~BackGestureWorkaround() override;

    void setWorkaroundParentItem(QQuickItem* item);
    QQuickItem* workaroundParentItem() const;

signals:
    void workaroundParentItemChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::mobile
