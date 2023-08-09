// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_button.h"

#include <QtQml/QtQml>

namespace nx::vms::client::core {

void CameraButton::registerQmlType()
{
    qmlRegisterUncreatableType<CameraButton>("nx.vms.client.core", 1, 0, "CameraButton",
        "Can't create CameraButton ");
}

bool CameraButton::instant() const
{
    return type == Type::instant;
}

bool CameraButton::prolonged() const
{
    return type == Type::prolonged;
}

bool CameraButton::checkable() const
{
    return type == Type::checkable;
}

CameraButton::Fields CameraButton::difference(const CameraButton& right) const
{
    Fields result = Field::empty;

    if (type != right.type)
        result |= Field::type;

    if (name != right.name)
        result |= Field::name;

    if (checkedName != right.checkedName)
        result |= Field::checkedName;

    if (hint != right.hint)
        result |= Field::hint;

    if (iconName != right.iconName)
        result |= Field::iconName;

    if (checkedIconName != right.checkedIconName)
        result |= Field::checkedIconName;

    if (enabled != right.enabled)
        result |= Field::enabled;

    if (checked != right.checked)
        result |= Field::checked;

    return result;
}

} // namespace nx::vms::client::core
