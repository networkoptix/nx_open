// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/enum_utils.h>

namespace nx::utils {
namespace test {

class EnumUtilsTest: public QObject
{
    Q_OBJECT

public:
    enum EnumType
    {
        Value1,
        Value2
    };
    Q_ENUM(EnumType)

    enum class EnumClassType
    {
        value1,
        value2
    };
    Q_ENUM(EnumClassType)

    enum EnumFlag
    {
        Flag1 = 0x1,
        Flag2 = 0x2
    };
    Q_FLAG(EnumFlag)

    enum class EnumClassFlag
    {
        flag1 = 0x1,
        flag2 = 0x2
    };
    Q_FLAG(EnumClassFlag)
};

namespace enum_utils_test {

Q_NAMESPACE

enum EnumType
{
    Value1,
    Value2
};
Q_ENUM_NS(EnumType)

enum class EnumClassType
{
    value1,
    value2
};
Q_ENUM_NS(EnumClassType)

enum EnumFlag
{
    Flag1 = 0x1,
    Flag2 = 0x2
};
Q_FLAG_NS(EnumFlag)

enum class EnumClassFlag
{
    flag1 = 0x1,
    flag2 = 0x2
};
Q_FLAG_NS(EnumClassFlag)

} // namespace enum_utils_test
} // namespace test
} // namespace nx::utils
