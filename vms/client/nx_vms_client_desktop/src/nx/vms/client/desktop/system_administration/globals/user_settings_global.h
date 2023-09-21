// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtGui/QColor>

#include <core/resource/resource_fwd.h>
#include <nx/vms/api/data/user_data.h>
#include <nx/vms/client/desktop/system_context.h>

namespace nx::vms::client::desktop {

class PasswordStrengthData
{
    Q_GADGET

    Q_PROPERTY(QString text MEMBER text)
    Q_PROPERTY(QString hint MEMBER hint)
    Q_PROPERTY(QColor background MEMBER background)
    Q_PROPERTY(bool accepted MEMBER accepted)

public:
    QString text;
    QString hint;
    QColor background;
    bool accepted = false;
};

class UserSettingsGlobal: public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString kUsersSection MEMBER kUsersSection CONSTANT)
    Q_PROPERTY(QString kGroupsSection MEMBER kGroupsSection CONSTANT)
    Q_PROPERTY(QString kBuiltInGroupsSection MEMBER kBuiltInGroupsSection CONSTANT)
    Q_PROPERTY(QString kCustomGroupsSection MEMBER kCustomGroupsSection CONSTANT)
    Q_PROPERTY(QString kLdapGroupsSection MEMBER kLdapGroupsSection CONSTANT)

public:
    UserSettingsGlobal();
    virtual ~UserSettingsGlobal();

    static const QString kUsersSection;
    static const QString kGroupsSection;
    static const QString kBuiltInGroupsSection;
    static const QString kCustomGroupsSection;
    static const QString kLdapGroupsSection;

    enum UserType
    {
        LocalUser = (int) nx::vms::api::UserType::local,
        CloudUser = (int) nx::vms::api::UserType::cloud,
        LdapUser = (int) nx::vms::api::UserType::ldap,
        TemporaryUser = (int) nx::vms::api::UserType::temporaryLocal,
    };
    Q_ENUM(UserType)

    Q_INVOKABLE QString validatePassword(const QString& password);
    Q_INVOKABLE nx::vms::client::desktop::PasswordStrengthData passwordStrength(
        const QString& password);

    Q_INVOKABLE QUrl accountManagementUrl() const;

    Q_INVOKABLE QString humanReadableSeconds(int seconds);

    static void registerQmlTypes();

    static UserType getUserType(const QnUserResourcePtr& user);
};

} // namespace nx::vms::client::desktop
