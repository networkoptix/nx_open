// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtCore/QVector>

#include <nx/vms/api/types/access_rights_types.h>

namespace nx::vms::client::desktop {

/** User-friendly information about an access right. */
struct AccessRightDescriptor
{
    nx::vms::api::AccessRight accessRight{};
    QString name;
    QString description;
    QUrl icon;

    Q_GADGET
    Q_PROPERTY(nx::vms::api::AccessRight accessRight MEMBER accessRight CONSTANT)
    Q_PROPERTY(QString name MEMBER name CONSTANT)
    Q_PROPERTY(QString description MEMBER description CONSTANT)
    Q_PROPERTY(QUrl icon MEMBER icon CONSTANT)
};

/** A singleton that provides user-friendly information about all possible access rights. */
class AccessRightsList: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVector<nx::vms::client::desktop::AccessRightDescriptor> items READ items CONSTANT)

public:
    static AccessRightsList* instance();
    static void registerQmlTypes();

    QVector<AccessRightDescriptor> items() const;
    AccessRightDescriptor get(nx::vms::api::AccessRight accessRight) const;

private:
    explicit AccessRightsList(QObject* parent = nullptr);

private:
    const QVector<AccessRightDescriptor> m_items;
    const QHash<nx::vms::api::AccessRight, int> m_indexLookup;
};

} // namespace nx::vms::client::desktop
