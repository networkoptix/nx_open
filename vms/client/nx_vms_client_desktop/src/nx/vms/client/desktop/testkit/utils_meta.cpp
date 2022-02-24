// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "utils.h"

#include <QtCore/QMetaObject>
#include <QtCore/QMetaMethod>

namespace nx::vms::client::desktop::testkit::utils {

QVariantMap getMetaInfo(const QMetaObject* meta, QString methodOrProperty)
{
    QVariantMap result;

    int i = meta->indexOfProperty(methodOrProperty.toUtf8().constData());
    if (i != -1)
    {
        result.insert("is_property", true);
        result.insert("type", meta->property(i).typeName());
        return result;
    }

    for (i = 0; i < meta->methodCount(); ++i)
    {
        auto method = meta->method(i);
        if (method.name() != methodOrProperty)
            continue;
        result.insert("is_method", true);
        result.insert("return_type", QMetaType::typeName(method.returnType()));
        return result;
    }

    return {};
}

} // namespace nx::vms::client::desktop::testkit::utils
