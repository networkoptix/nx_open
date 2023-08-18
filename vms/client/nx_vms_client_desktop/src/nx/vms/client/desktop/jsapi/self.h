// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

class QnWorkbenchItem;

namespace nx::vms::client::desktop::jsapi {

/**
 * Self object provides functionality related to the current web page item.
 */
class Self: public QObject
{
    Q_OBJECT

public:
    Self(QnWorkbenchItem* item, QObject* parent = nullptr);
    virtual ~Self() override;

    /** Sets the web page widget minimal interface mode. */
    Q_INVOKABLE QJsonObject setMinimalInterfaceMode(bool value);

    /** Prevent showing default context menu. */
    Q_INVOKABLE QJsonObject setPreventDefaultContextMenu(bool value);

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop::jsapi
