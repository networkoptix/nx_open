// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/common/system_context_aware.h>

#include "../action_builder_field.h"

namespace nx::vms::rules {

constexpr auto kCameraFullScreenValidationPolicy = "cameraFullScreen";
constexpr auto kExecPtzValidationPolicy = "execPtz";

struct TargetSingleDeviceFieldProperties
{
    FieldProperties base;

    /** Value for the just created field's `id` property. */
    nx::Uuid id;

    /** Value for the just created field's `useSource` property. */
    bool useSource{false};

    bool allowEmptySelection{false};
    QString validationPolicy;

    QVariantMap toVariantMap() const
    {
        QVariantMap result = base.toVariantMap();

        result.insert("id", QVariant::fromValue(id));
        result.insert("useSource", useSource);
        result.insert("allowEmptySelection", allowEmptySelection);
        result.insert("validationPolicy", validationPolicy);
        return result;
    }

    static TargetSingleDeviceFieldProperties fromVariantMap(const QVariantMap& properties)
    {
        TargetSingleDeviceFieldProperties result;

        result.id = properties.value("id").value<nx::Uuid>();
        result.useSource = properties.value("useSource").toBool();

        result.base = FieldProperties::fromVariantMap(properties);
        result.allowEmptySelection = properties.value("allowEmptySelection", false).toBool();
        result.validationPolicy = properties.value("validationPolicy").toString();

        return result;
    }
};

/** Displayed as a device selection button and a "Use source" checkbox. */
class NX_VMS_RULES_API TargetDeviceField: public ActionBuilderField
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "device")

    Q_PROPERTY(nx::Uuid id READ id WRITE setId NOTIFY idChanged)
    Q_PROPERTY(bool useSource READ useSource WRITE setUseSource NOTIFY useSourceChanged)

public:
    using ActionBuilderField::ActionBuilderField;

    nx::Uuid id() const;
    void setId(nx::Uuid id);
    bool useSource() const;
    void setUseSource(bool value);

    virtual QVariant build(const AggregatedEventPtr& eventAggregator) const override;

    TargetSingleDeviceFieldProperties properties() const;
    static QJsonObject openApiDescriptor(const QVariantMap& properties);

signals:
    void idChanged();
    void useSourceChanged();

private:
    nx::Uuid m_id;
    bool m_useSource{false};
};

} // namespace nx::vms::rules
