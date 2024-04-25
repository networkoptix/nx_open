// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <vector>

#include <QtCore/QObject>

#include <nx/utils/uuid.h>

class QDateTime;

namespace nx::vms::rules {

class ActionBuilder;
class EventFilter;
class Engine;

/**
 * VMS rule description.
 */
class NX_VMS_RULES_API Rule: public QObject
{
    Q_OBJECT

public:
    explicit Rule(nx::Uuid id, const Engine* engine);
    virtual ~Rule() override;

    nx::Uuid id() const;
    void setId(nx::Uuid id);

    const Engine* engine() const;

    // Takes ownership.
    void addEventFilter(std::unique_ptr<EventFilter> filter);

    // Takes ownership.
    void addActionBuilder(std::unique_ptr<ActionBuilder> builder);

    // Takes ownership.
    void insertEventFilter(int index, std::unique_ptr<EventFilter> filter);

    // Takes ownership.
    void insertActionBuilder(int index, std::unique_ptr<ActionBuilder> builder);

    std::unique_ptr<EventFilter> takeEventFilter(int idx);
    std::unique_ptr<ActionBuilder> takeActionBuilder(int idx);

    // TODO: #spanasenko const pointers?
    const QList<EventFilter*> eventFilters() const;
    const QList<EventFilter*> eventFiltersByType(const QString& type) const;
    const QList<ActionBuilder*> actionBuilders() const;

    void setComment(const QString& comment);
    QString comment() const;

    void setEnabled(bool isEnabled);
    bool enabled() const;

    bool isSystem() const;
    void setSystem(bool system);

    void setSchedule(const QByteArray& schedule);
    QByteArray schedule() const;
    bool timeInSchedule(QDateTime time) const;

    bool isValid() const;

    void connectSignals();

signals:
    void stateChanged();

private:
    void updateState();

private:
    nx::Uuid m_id;
    const Engine* m_engine;

    // TODO: #spanasenko and-or logic
    std::vector<std::unique_ptr<EventFilter>> m_filters;
    std::vector<std::unique_ptr<ActionBuilder>> m_builders;

    QString m_comment;
    bool m_enabled = true;
    bool m_system = false;
    QByteArray m_schedule;

    bool m_updateInProgress = false;
};

NX_VMS_RULES_API bool operator==(const Rule& left, const Rule& right);

} // namespace nx::vms::rules
