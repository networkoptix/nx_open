// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <vector>

#include <QtCore/QString>

#include <nx/utils/uuid.h>

namespace nx::vms::client::core {

struct NX_VMS_CLIENT_CORE_API CameraButton
{
    Q_GADGET
    Q_PROPERTY(QnUuid id MEMBER id)
    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(QString hint MEMBER hint)
    Q_PROPERTY(QString iconName MEMBER iconName)
    Q_PROPERTY(Type type MEMBER type)
    Q_PROPERTY(bool enabled MEMBER enabled)
    Q_PROPERTY(Group group MEMBER group)

public:
    static void registerQmlType();

    enum class Type
    {
        instant,
        prolonged,
        checkable
    };
    Q_ENUM(Type)

    enum class Field
    {
        empty = 0x0,
        name = 0x1,
        checkedName = 0x2,
        hint = 0x4,
        iconName = 0x8,
        checkedIconName = 0x10,
        type = 0x20,
        enabled = 0x40,
        checked = 0x80
    };
    Q_DECLARE_FLAGS(Fields, Field)

    using Group = int;

    QnUuid id;
    QString name;
    QString checkedName;
    QString hint;
    QString iconName;
    QString checkedIconName;
    Type type = Type::instant;
    bool enabled = true;
    bool checked = false;

    /**
     * As buttons could be populated from the different controllers we mark each one by some
     * controller identifier. Can be used in sorting operations.
     */
    Group group = 0;

    bool instant() const;
    bool prolonged() const;
    bool checkable() const;
    Fields difference(const CameraButton& right) const;
};

using CameraButtons = std::vector<CameraButton>;
using OptionalCameraButton = std::optional<CameraButton>;

} // namespace nx::vms::client::core
