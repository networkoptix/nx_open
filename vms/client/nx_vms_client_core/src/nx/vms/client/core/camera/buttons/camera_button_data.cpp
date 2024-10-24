// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_button_data.h"

#include <QtQml/QtQml>

namespace nx::vms::client::core {

void CameraButtonData::registerQmlType()
{
    qmlRegisterUncreatableType<CameraButtonData>("nx.vms.client.core", 1, 0, "CameraButton",
        "Can't create CameraButton ");
}

bool CameraButtonData::instant() const
{
    return type == Type::instant;
}

bool CameraButtonData::prolonged() const
{
    return type == Type::prolonged;
}

bool CameraButtonData::checkable() const
{
    return type == Type::checkable;
}

CameraButtonData::Fields CameraButtonData::difference(const CameraButtonData& right) const
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
