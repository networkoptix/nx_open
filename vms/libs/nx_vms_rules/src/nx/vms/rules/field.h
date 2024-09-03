// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QMetaClassInfo>
#include <QtCore/QObject>

#include <nx/utils/string.h>
#include <nx/utils/uuid.h>

#include "rules_fwd.h"

namespace nx::vms::rules {

/** Common field properties that will be stored in the filed manifest. */
struct FieldProperties
{
    /** Whether given field should be visible in the editor. */
    bool visible{true};

    QVariantMap toVariantMap() const
    {
        return {{"visible", visible}};
    }

    static FieldProperties fromVariantMap(const QVariantMap& properties)
    {
        FieldProperties result;
        result.visible = properties.value("visible", true).toBool();

        return result;
    }
};

/**
 * Base class for storing actual rule values for event filters and action builders.
 */
class NX_VMS_RULES_API Field: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    static constexpr auto kMetatype = "metatype";
    static constexpr auto kEncrypt = "encrypt";

public:
    explicit Field(const FieldDescriptor* descriptor);

    QString metatype() const;

    /** Full properties representation. */
    QMap<QString, QJsonValue> serializedProperties() const;

    const FieldDescriptor* descriptor() const;

    /**
     * Set field properties.
     * @return Whether all the properties is set successfully.
     */
    bool setProperties(const QVariantMap& properties);

    FieldProperties properties() const;

private:
    const FieldDescriptor* m_descriptor = nullptr;
};

template <class T>
QString fieldMetatype()
{
    const auto& meta = T::staticMetaObject;
    int idx = meta.indexOfClassInfo(Field::kMetatype);

    return (idx < 0) ? QString() : meta.classInfo(idx).value();
}

template <class T>
QSet<QString> encryptedProperties()
{
    const auto& meta = T::staticMetaObject;
    const auto idx = meta.indexOfClassInfo(Field::kEncrypt);
    if (idx < 0)
        return {};

    QSet<QString> result;
    for (const auto& s: QString(meta.classInfo(idx).value()).split(',', Qt::SkipEmptyParts))
        result.insert(s.trimmed());

    return result;
}

} // namespace nx::vms::rules
