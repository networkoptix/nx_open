// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>

#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::mobile::details {

enum PushPermission
{
    unknown, //< User haven't picked permissions or we haven't get them yet.
    denied, //< User denied push notifications.
    authorized //< User allows push notifications.
};

/**
 * Handle OS push notification permission changes.
 */
class PushPermissionManager: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_PROPERTY(PushPermission permission
        READ permission
        NOTIFY permissionChanged)

public:
    PushPermissionManager(QObject* parent = nullptr);
    virtual ~PushPermissionManager() override;

    PushPermission permission() const;

signals:
    void permissionChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::mobile::details

Q_DECLARE_METATYPE(nx::vms::client::mobile::details::PushPermission)
