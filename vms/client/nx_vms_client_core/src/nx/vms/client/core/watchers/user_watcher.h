// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>
#include <utils/common/connective.h>

namespace nx::vms::client::core {

class UserWatcher: public Connective<QObject>, public CommonModuleAware
{
    Q_OBJECT
    /* This property should remain read-only for QML! */
    Q_PROPERTY(QString userName READ userName NOTIFY userNameChanged)

    using base_type = Connective<QObject>;

public:
    UserWatcher(QObject* parent = nullptr);

    void setUser(const QnUserResourcePtr& currentUser);
    const QnUserResourcePtr& user() const;

    void setUserName(const QString& name);
    const QString& userName() const;

signals:
    void userChanged(const QnUserResourcePtr& user);
    void userNameChanged();

private:

private:
    QString m_userName;
    QnUserResourcePtr m_user;
};

} // namespace nx::vms::client::core
