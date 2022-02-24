// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaObject>
#include <QtCore/QObject>
#include <QtCore/QVariantList>

#include <nx/utils/log/assert.h>

namespace nx::vms::client::core {

template<typename T>
QList<T> toTypedList(const QVariantList& source)
{
    QList<T> result;
    for (const QVariant& item: source)
        result << item.value<T>();
    return result;
}

template<typename ResultType, typename... ArgTypes>
ResultType invokeQmlMethod(QObject* object, const char* methodName, ArgTypes... args)
{
    if constexpr (std::is_void_v<ResultType>)
    {
        NX_ASSERT(QMetaObject::invokeMethod(object, methodName, Qt::DirectConnection,
            Q_ARG(QVariant, QVariant::fromValue(args))...));
    }
    else
    {
        QVariant result;
        NX_ASSERT(QMetaObject::invokeMethod(object, methodName, Qt::DirectConnection,
            Q_RETURN_ARG(QVariant, result),
            Q_ARG(QVariant, QVariant::fromValue(args))...));

        return result.value<ResultType>();
    }
}

} // namespace nx::vms::client::core
