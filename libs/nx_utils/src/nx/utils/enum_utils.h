// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaEnum>
#include <QtCore/QString>

#include <nx/utils/log/format.h>

namespace nx::utils {

/**
 * Returns unqualified enum or enum class constant name by its value.
 * The enum must be registered with the Q_ENUM macro.
 */
template<typename EnumType>
inline QString enumToShortString(EnumType value)
{
    return QMetaEnum::fromType<EnumType>().valueToKey(int(value));
}

/**
 * Returns enum or enum class constant name by its value, qualified with the enum name.
 * The enum must be registered with the Q_ENUM macro.
 */
template<typename EnumType>
inline QString enumToString(EnumType value)
{
    const auto metaEnum = QMetaEnum::fromType<EnumType>();
    return nx::format("%1::%2", metaEnum.enumName(), metaEnum.valueToKey(int(value)));
}

/**
 * Returns enum or enum class constant name by its value, fully qualified.
 * The enum must be registered with the Q_ENUM macro.
 */
template<typename EnumType>
inline QString enumToFullString(EnumType value)
{
    const auto metaEnum = QMetaEnum::fromType<EnumType>();
    return nx::format("%1::%2::%3", metaEnum.scope(), metaEnum.enumName(),
        metaEnum.valueToKey(int(value)));
}

} // namespace nx::utils
