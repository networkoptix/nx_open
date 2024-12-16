// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <memory>

#include <QtCore/QObject>

#include <nx/utils/uuid.h>

#include "rules_fwd.h"

namespace nx::vms::rules {

class EventFilterField;

/**
 * Event filters are used to filter events received by VMS rules engine
 * on the basis of event type and field values. Events that passed the filter
 * are proceed by the engine and lead to execution of actions from the same rule.
 */
class NX_VMS_RULES_API /*FieldBased*/EventFilter: public QObject
{
    Q_OBJECT

public:
    EventFilter(nx::Uuid id, const QString& eventType);
    virtual ~EventFilter() override;

    nx::Uuid id() const;
    QString eventType() const;

    const Rule* rule() const;
    void setRule(const Rule* rule);

    // Takes ownership.
    void addField(const QString& name, std::unique_ptr<EventFilterField> field);

    QHash<QString, EventFilterField*> fields() const;

    /** Match all event fields excluding state. */
    bool matchFields(const EventPtr& event) const;
    /** Match state field only. */
    bool matchState(const EventPtr& event) const;

    template<class T = Field>
    const T* fieldByName(const QString& name) const
    {
        return fieldByNameImpl<T>(name);
    }

    template<class T = Field>
    T* fieldByName(const QString& name)
    {
        return fieldByNameImpl<T>(name);
    }

    template<class T>
    const T* fieldByType() const
    {
        const auto result = fieldsByTypeImpl<T>(/*stopOnFirstHit*/ true);
        return result.empty() ? nullptr : result.front();
    }

    template<class T>
    T* fieldByType()
    {
        const auto result = fieldsByTypeImpl<T>(/*stopOnFirstHit*/ true);
        return result.empty() ? nullptr : result.front();
    }

    template<class T>
    std::vector<const T*> fieldsByType() const
    {
        auto fields = fieldsByTypeImpl<T>(/*stopOnFirstHit*/ false);
        return std::vector<const T*>(fields.begin(), fields.end());
    }

    template<class T>
    std::vector<T*> fieldsByType()
    {
        return fieldsByTypeImpl<T>(/*stopOnFirstHit*/ false);
    }

signals:
    /** Emitted whenever any field property is changed. */
    void changed(const QString& fieldName);

private slots:
    void onFieldChanged();

private:
    template<class T>
    T* fieldByNameImpl(const QString& name) const
    {
        const auto it = m_fields.find(name);
        return (it == m_fields.end()) ? nullptr : dynamic_cast<T*>(it->second.get());
    }

    /**
     * Returns fields with the given type T. If stopOnFirstHit is set, returns only
     * the first matched field.
     */
    template<class T>
    std::vector<T*> fieldsByTypeImpl(bool stopOnFirstHit) const
    {
        std::vector<T*> result;
        for (const auto& [_, field]: m_fields)
        {
            if (auto targetField = dynamic_cast<T*>(field.get()))
            {
                result.push_back(targetField);

                if (stopOnFirstHit)
                    break;
            }
        }

        return result;
    }

    nx::Uuid m_id;
    QString m_eventType;
    const Rule* m_rule = {};
    std::map<QString, std::unique_ptr<EventFilterField>> m_fields;
};

} // namespace nx::vms::rules
