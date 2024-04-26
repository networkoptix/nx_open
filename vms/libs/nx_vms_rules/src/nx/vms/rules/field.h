// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaClassInfo>
#include <QtCore/QObject>

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

public:
    explicit Field(const FieldDescriptor* descriptor);

    QString metatype() const;

    void connectSignals();

    QMap<QString, QJsonValue> serializedProperties() const;

    const FieldDescriptor* descriptor() const;

    /**
     * Set field properties.
     * @return Whether all the properties is set successfully.
     */
    bool setProperties(const QVariantMap& properties);

    bool event(QEvent* ev) override;

    FieldProperties properties() const;

signals:
    void stateChanged();

private:
    void notifyParent();

private:
    const FieldDescriptor* m_descriptor = nullptr;
    bool m_connected = false;
    bool m_updateInProgress = false;
};

template <class T>
QString fieldMetatype()
{
    const auto& meta = T::staticMetaObject;
    int idx = meta.indexOfClassInfo(Field::kMetatype);

    return (idx < 0) ? QString() : meta.classInfo(idx).value();
}

} // namespace nx::vms::rules
