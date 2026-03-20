// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "field.h"

#include <nx/utils/log/log.h>
#include <nx/utils/qobject.h>

#include "manifest.h"
#include "utils/serialization.h"
#include "utils/type.h"

namespace nx::vms::rules {

Field::Field(const FieldDescriptor* descriptor): m_descriptor{descriptor}
{
    NX_ASSERT(m_descriptor, "Field descriptor is required");
}

QString Field::type() const
{
    QString result = utils::type(metaObject());
    NX_ASSERT(!result.isEmpty(), "Field type is invalid");
    return result;
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
    for (const auto& propertyName: nx::utils::propertyNames(this))
    {
        const auto propertyIt = properties.constFind(propertyName);
        if (propertyIt == properties.constEnd()) //< Only declared properties must be set.
            continue;

        if (!setProperty(propertyName, propertyIt.value()))
        {
            NX_ERROR(this, "Failed to set property %1 for the %2 field", propertyName, type());
            isAllPropertiesSet = false;
        }
    }

    return isAllPropertiesSet;
}

FieldProperties Field::properties() const
{
    return FieldProperties::fromVariantMap(descriptor()->properties);
}

bool Field::match(const QRegularExpression& /*pattern*/) const
{
    return false;
}

} // namespace nx::vms::rules
