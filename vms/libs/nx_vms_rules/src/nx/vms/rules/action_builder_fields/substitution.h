// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../action_builder_field.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API Substitution: public ActionBuilderField
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.fields.substitution")

    Q_PROPERTY(QString fieldName READ fieldName WRITE setFieldName NOTIFY fieldNameChanged)

public:
    using ActionBuilderField::ActionBuilderField;

    virtual QVariant build(const AggregatedEventPtr& eventAggregator) const override;

    QString fieldName() const;
    void setFieldName(const QString& fieldName);

signals:
    void fieldNameChanged();

private:
    QString m_fieldName;
};

} // namespace nx::vms::rules
