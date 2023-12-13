// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/impl_ptr.h>
#include <nx/vms/common/system_context_aware.h>

#include "../action_builder_field.h"

namespace nx::vms::rules {

 /** Perform string formatting using event data values. */
class NX_VMS_RULES_API TextWithFields:
    public ActionBuilderField,
    public common::SystemContextAware
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.fields.textWithFields")

    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)

public:
    explicit TextWithFields(common::SystemContext* context);
    virtual ~TextWithFields() override;

    virtual QVariant build(const AggregatedEventPtr& eventAggregator) const override;

    QString text() const;
    void setText(const QString& text);

    void parseText();

    enum class FieldType
    {
        Text,
        Substitution
    };

    struct ValueDescriptor
    {
        FieldType type;
        QString value;
        qsizetype start = 0;
        qsizetype length = 0;
        bool correct = false;
    };

    using ParsedValues = QList<ValueDescriptor>;

    static const QStringList& availableFunctions();

signals:
    void textChanged();
    void parseFinished(const ParsedValues & parsedValues);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

/** Same as base class but hidden from user. */
class NX_VMS_RULES_API TextFormatter: public TextWithFields
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.actions.fields.textFormatter")

public:
    using TextWithFields::TextWithFields;
};

} // namespace nx::vms::rules
