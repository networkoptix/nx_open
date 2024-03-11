// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaClassInfo>
#include <QtCore/QMetaObject>
#include <QtCore/QObject>

#include <nx/utils/uuid.h>

namespace nx::vms::rules::utils {

static constexpr auto kType = "type";

inline QString metaInfo(const QMetaObject* metaObject, const char * name)
{
    auto idx = (metaObject && name) ? metaObject->indexOfClassInfo(name) : -1;

    return (idx < 0) ? QString() : metaObject->classInfo(idx).value();
}

inline QString type(const QMetaObject* metaObject)
{
    return metaInfo(metaObject, kType);
}

template<class T>
QString type()
{
    return type(&T::staticMetaObject);
}

template<class... Strings>
QString makeName(const Strings&... strings)
{
    static constexpr auto kKeyNameSeparator = '_';

    return QStringList{strings...}.join(kKeyNameSeparator);
}

} // namespace nx::vms::rules::utils
