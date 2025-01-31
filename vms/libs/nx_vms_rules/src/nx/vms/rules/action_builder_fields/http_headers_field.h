// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../base_fields/simple_type_field.h"
#include "../field_types.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API HttpHeadersField:
    public SimpleTypeActionField<QList<KeyValueObject>, HttpHeadersField>
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "httpHeaders")

    Q_PROPERTY(QList<KeyValueObject> value READ value WRITE setValue NOTIFY valueChanged)

public:
    using SimpleTypeActionField<QList<KeyValueObject>, HttpHeadersField>::SimpleTypeActionField;

    static QJsonObject openApiDescriptor(const QVariantMap& properties);

signals:
    void valueChanged();
};

} // namespace nx::vms::rules
