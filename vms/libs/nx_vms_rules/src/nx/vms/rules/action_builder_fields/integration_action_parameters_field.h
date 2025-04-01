// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>

#include <nx/vms/common/system_context_aware.h>
#include <nx/utils/uuid.h>
#include <nx/vms/rules/utils/text_tokenizer.h>

#include "../action_builder_field.h"

namespace nx::vms::rules {

/**
 * Field for the parameters of Integration Custom Action, described in its manifest. Displayed as
 * a button which opens a dialog with a form to fill in.
 */
class NX_VMS_RULES_API IntegrationActionParametersField:
    public ActionBuilderField,
    public common::SystemContextAware
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "integrationActionParameters")

    Q_PROPERTY(QJsonObject value READ value WRITE setValue NOTIFY valueChanged)

public:
    IntegrationActionParametersField(
        common::SystemContext* context, const FieldDescriptor* descriptor);
    virtual ~IntegrationActionParametersField() override;

    virtual QVariant build(const AggregatedEventPtr& eventAggregator) const override;

    QJsonObject value() const;
    void setValue(const QJsonObject& value);

    static QJsonObject openApiDescriptor(const QVariantMap& properties);

signals:
    void valueChanged();

private:
    QJsonObject m_rawValue;
    std::map<QString, std::variant<QJsonValue, utils::TextTokenList>> m_parsedValue;
};

} // namespace nx::vms::rules
