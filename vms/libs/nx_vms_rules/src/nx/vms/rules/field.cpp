// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "field.h"

#include <QtCore/QEvent>
#include <QtCore/QMetaProperty>
#include <QtCore/QScopedValueRollback>

#include <nx/utils/log/log.h>
#include <nx/utils/qobject.h>

#include "manifest.h"
#include "utils/serialization.h"

namespace nx::vms::rules {

Field::Field(const FieldDescriptor* descriptor): m_descriptor{descriptor}
{
    NX_ASSERT(m_descriptor, "Field descriptor is required");
}

QString Field::metatype() const
{
    int idx = metaObject()->indexOfClassInfo(kMetatype);
    // NX_ASSERT(idx >= 0, "Class does not have required 'metatype' property");

    return metaObject()->classInfo(idx).value();
}

QMap<QString, QJsonValue> Field::serializedProperties() const
{
    return serializeProperties(this, nx::utils::propertyNames(this));
}

const FieldDescriptor* Field::descriptor() const
{
    NX_ASSERT(m_descriptor);
    return m_descriptor;
}

bool Field::setProperties(const QVariantMap& properties)
{
    bool isAllPropertiesSet = true;
    for (const auto& propertyName: utils::propertyNames(this))
    {
        const auto propertyIt = properties.constFind(propertyName);
        if (propertyIt == properties.constEnd()) //< Only declared properties must be set.
            continue;

        if (!setProperty(propertyName, propertyIt.value()))
        {
            NX_ERROR(this, "Failed to set property %1 for the %2 field", propertyName, metatype());
            isAllPropertiesSet = false;
        }
    }

    return isAllPropertiesSet;
}

FieldProperties Field::properties() const
{
    return FieldProperties::fromVariantMap(descriptor()->properties);
}

} // namespace nx::vms::rules
