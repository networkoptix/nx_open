// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtQml/QQmlEngine>

namespace nx::vms::client::core {

template<typename ObjectPtr>
typename ObjectPtr::value_type* withCppOwnership(const ObjectPtr& pointer)
{
    if (!pointer)
        return {};

    QQmlEngine::setObjectOwnership(pointer.get(), QQmlEngine::CppOwnership);
    return pointer.get();
}

template<typename T>
T* withCppOwnership(T* pointer)
{
    if (!pointer)
        return {};

    QQmlEngine::setObjectOwnership(pointer, QQmlEngine::CppOwnership);
    return pointer;
}

} // namespace nx::vms::client::core
