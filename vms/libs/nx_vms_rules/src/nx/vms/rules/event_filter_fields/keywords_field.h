// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../event_filter_field.h"

namespace nx::vms::rules {

/**
 * Field matches event string with filter keywords separated by space.
 * Any keyword match is enough. Empty keywords are always match.
 */
class NX_VMS_RULES_API KeywordsField: public EventFilterField
{
    Q_OBJECT

    Q_CLASSINFO("metatype", "nx.events.fields.keywords")

    Q_PROPERTY(QString string READ string WRITE setString NOTIFY stringChanged)

public:
    using EventFilterField::EventFilterField;

    virtual bool match(const QVariant& value) const override;

    QString string() const;
    void setString(const QString& string);
    static QJsonObject openApiDescriptor();

signals:
    void stringChanged();

private:
    QString m_string;
    QStringList m_list;
};

} // namespace nx::vms::rules
