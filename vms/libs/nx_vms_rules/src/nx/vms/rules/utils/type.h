// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QMetaObject>

namespace nx::vms::rules::utils {

static constexpr auto kType = "type";

inline QString type(const QMetaObject* metaObject)
{
    constexpr int kInvalidPropertyIndex = -1;
    int idx = metaObject ? metaObject->indexOfClassInfo(kType) : kInvalidPropertyIndex;
    return (idx < 0) ? QString() : metaObject->classInfo(idx).value();
}

template<class T>
QString type()
{
    return type(&T::staticMetaObject);
}


} // namespace nx::vms::rules::utils
