// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/data/lookup_list_data.h>
#include <nx/vms/common/system_context_aware.h>

#include "../event_filter_field.h"
#include "../field_types.h"

namespace nx::vms::rules {

/*
 * Field checks if the provided value belongs or not to the defined entity. There are two types of
 * entities. The first one is keywords - string separated by space. The second one is lookup list.
 */
class NX_VMS_RULES_API LookupField:
    public EventFilterField,
    public common::SystemContextAware
{
    Q_OBJECT
    Q_CLASSINFO("metatype", "nx.events.fields.lookup")

    Q_PROPERTY(QString value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(LookupCheckType checkType READ checkType WRITE setCheckType NOTIFY checkTypeChanged)
    Q_PROPERTY(LookupSource source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(QStringList attributes READ attributes WRITE setAttributes NOTIFY attributesChanged)

public:
    explicit LookupField(common::SystemContext* context);

    /*
     * Source dependent value. If the source is keywords - space separated string,
     * lookup list id otherwise.
     */
    QString value() const;
    void setValue(const QString& value);

    LookupCheckType checkType() const;
    void setCheckType(LookupCheckType type);
    LookupSource source() const;
    void setSource(LookupSource source);

    /* Selected attributes used for a match check. If empty, check by all the source attributes. */
    QStringList attributes() const;
    void setAttributes(const QStringList& attributes);

    bool match(const QVariant& eventValue) const override;

signals:
    void valueChanged();
    void checkTypeChanged();
    void sourceChanged();
    void attributesChanged();

private:
    QString m_value;
    LookupCheckType m_checkType{LookupCheckType::in};
    LookupSource m_source{LookupSource::keywords};
    QStringList m_attributes;

    mutable std::optional<vms::api::LookupListData> m_lookupList;

    bool match(const std::map<QString, QString>& eventData) const;
    bool match(const QString& eventValue, const QString& entryValue) const;
};

} // namespace nx::vms::rules
