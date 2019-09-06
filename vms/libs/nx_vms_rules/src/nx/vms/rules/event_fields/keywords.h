#pragma once

#include "../event_field.h"

namespace nx::vms::rules {

class NX_VMS_RULES_API Keywords: public EventField
{
    Q_OBJECT

    Q_PROPERTY(QString string READ string WRITE setString)

public:
    Keywords();

    QString metatype() const final override { return "nx.keywords"; }

    virtual bool match(const QVariant& value) const override;

    QString string() const;
    void setString(const QString& string);

private:
    QString m_string;
    QStringList m_list;
};

} // namespace nx::vms::rules
