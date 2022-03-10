// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <memory>

#include <QtCore/QObject>
#include <QtCore/QTimer>

#include <nx/utils/uuid.h>

#include "rules_fwd.h"

namespace nx::vms::rules {

class ActionField;
class BasicAction;

/**
 * Action builders are used to construct actual action parameters from action settings
 * contained in rules, and fields of events that triggered these rules.
 */
class NX_VMS_RULES_API /*FieldBased*/ActionBuilder: public QObject
{
    Q_OBJECT

public:
    using ActionConstructor = std::function<BasicAction*()>;

    ActionBuilder(const QnUuid& id, const QString& actionType, const ActionConstructor& ctor);
    virtual ~ActionBuilder();

    QnUuid id() const;
    QString actionType() const;

    /**
     * Get all configurable Builder properties in a form of flattened dictionary,
     * where each key has name in a format "field_name/field_property_name".
     */
    std::map<QString, QVariant> flatData() const;

    /**
     * Update single Builder property.
     * @path Path of the property, should has "field_name/field_property_name" format.
     * @value Value that should be assigned to the given property.
     * @return True on success, false if such field does not exist.
     */
    bool updateData(const QString& path, const QVariant& value);

    /**
     * Update several Builder properties at once.
     * Does nothing if any of the keys is invalid (e.g. such field does not exist).
     * @data Dictionary of values.
     * @return True on success, false otherwise.
     */
    bool updateFlatData(const std::map<QString, QVariant>& data);

    // Takes ownership.
    void addField(const QString& name, std::unique_ptr<ActionField> field);

    const QHash<QString, ActionField*> fields() const;

    QSet<QString> requiredEventFields() const;
    QSet<QnUuid> affectedResources(const EventData& eventData) const;

    ActionPtr process(const EventData& eventData);

    void setAggregationInterval(std::chrono::seconds interval);
    std::chrono::seconds aggregationInterval() const;

    void connectSignals();

signals:
    void stateChanged();

private:
    void onTimeout();
    void updateState();

private:
    QnUuid m_id;
    QString m_actionType;
    ActionConstructor m_constructor;
    std::map<QString, std::unique_ptr<ActionField>> m_fields;
    QList<ActionField*> m_targetFields;
    std::chrono::seconds m_interval = std::chrono::seconds(0);
    QTimer m_timer;
    bool m_updateInProgress = false;
};

} // namespace nx::vms::rules