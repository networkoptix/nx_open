// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <vector>

#include <QtCore/QObject>

#include <nx/utils/uuid.h>

namespace nx::vms::rules {

class EventFilter;
class ActionBuilder;

/**
 * VMS rule description.
 */
class NX_VMS_RULES_API Rule: public QObject
{
    Q_OBJECT

public:
    explicit Rule(const QnUuid& id);
    ~Rule();

    QnUuid id() const;

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
    const QList<ActionBuilder*> actionBuilders() const;

    void setComment(const QString& comment);
    QString comment() const;

    void setEnabled(bool isEnabled);
    bool enabled() const;

    void setSchedule(const QByteArray& schedule);
    QByteArray schedule() const;

    void connectSignals();

signals:
    void stateChanged();

private:
    void updateState();

private:
    QnUuid m_id;

    // TODO: #spanasenko and-or logic
    std::vector<std::unique_ptr<EventFilter>> m_filters;
    std::vector<std::unique_ptr<ActionBuilder>> m_builders;

    QString m_comment;
    bool m_enabled;
    QByteArray m_schedule;

    bool m_updateInProgress = false;
};

} // namespace nx::vms::rules
