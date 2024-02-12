// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QVariant>

#include <nx/vms/event/action_parameters.h>
#include <nx/vms/event/event_parameters.h>
#include <ui/workbench/workbench_context_aware.h>

class QStandardItemModel;

namespace nx::vms::event { class StringsHelper; }

class QnBusinessRuleViewModel: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    enum class Column
    {
        modified,
        disabled,
        event,
        source,
        spacer,
        action,
        target,
        aggregation
    };

    static QList<Column> allColumns();

    enum class Field
    {
        modified = 0x00000001,
        eventType = 0x00000002,
        eventResources = 0x00000004,
        eventParams = 0x00000008,
        eventState = 0x00000010,
        actionType = 0x00000020,
        actionResources = 0x00000040,
        actionParams = 0x00000080,
        aggregation = 0x00000100,
        disabled = 0x00000200,
        comments = 0x00000400,
        schedule = 0x00000800,
        all = 0x0000FFFF
    };
    Q_DECLARE_FLAGS(Fields, Field)

public:
    QnBusinessRuleViewModel(QObject *parent = 0);
    virtual ~QnBusinessRuleViewModel();

    QVariant data(Column column, const int role = Qt::DisplayRole) const;
    bool setData(Column column, const QVariant& value, int role);

    void loadFromRule(const nx::vms::event::RulePtr& businessRule);
    nx::vms::event::RulePtr createRule() const;

    QString getText(Column column, bool detailed = true) const;
    QString getToolTip(Column column) const;
    QIcon getIcon(Column column) const;
    int getHelpTopic(Column column) const;

    bool isValid(Column column) const;
    bool isValid() const; //checks validity for entire row

    nx::Uuid id() const;

    bool isModified() const;
    void setModified(bool value);

    nx::vms::api::EventType eventType() const;
    void setEventType(const nx::vms::api::EventType value);

    QSet<nx::Uuid> eventResources() const;
    void setEventResources(const QSet<nx::Uuid>& value);

    nx::vms::event::EventParameters eventParams() const;
    void setEventParams(const nx::vms::event::EventParameters& params);

    nx::vms::api::EventState eventState() const;
    void setEventState(nx::vms::api::EventState state);

    nx::vms::api::ActionType actionType() const;
    void setActionType(const nx::vms::api::ActionType value);

    QSet<nx::Uuid> actionResources() const;
    void setActionResources(const QSet<nx::Uuid>& value);

    QSet<nx::Uuid> actionResourcesRaw() const;
    void setActionResourcesRaw(const QSet<nx::Uuid>& value);

    bool isActionProlonged() const;

    nx::vms::event::ActionParameters actionParams() const;
    void setActionParams(const nx::vms::event::ActionParameters& params);

    int aggregationPeriod() const;
    void setAggregationPeriod(int seconds);

    bool disabled() const;
    void setDisabled(bool value);

    bool actionCanUseSourceCamera() const;
    bool isActionUsingSourceCamera() const;
    QString sourceCameraCheckboxText() const;
    void toggleActionUseSourceCamera();

    bool actionCanUseSourceServer() const;
    bool actionIsUsingSourceServer() const;
    void toggleActionUseSourceServer();


    QIcon iconForAction() const;

    /**
     * Get text for the Source field.
     * @param detailed Detailed text is used in the table cell.
     *   Not detailed - as the button caption and in the advanced view.
     * @return Formatted text.
     */
    QString getSourceText(bool detailed) const;
    QString getTargetText(bool detailed) const;

    QString schedule() const;

    /**
    * @param value Binary string encoded as HEX. Each bit represent 1 hour of week schedule.
    * First 24*7 bits is used. Rest of the string is ignored.
    * First day of week is Monday independent of system settings.
    */
    void setSchedule(const QString& value);

    QString comments() const;
    void setComments(const QString& value);

    QStandardItemModel* eventTypesModel();
    QStandardItemModel* eventStatesModel();
    QStandardItemModel* actionTypesModel();

public:
    // TODO: #vkutin #3.2 Temporary workaround to pass "all users" as a special uuid.
    static const nx::Uuid kAllUsersId;

signals:
    void dataChanged(Fields fields);

private:
    void updateActionTypesModel();
    void updateEventStateModel();

    QString getAggregationText() const;

    static QString toggleStateToModelString(nx::vms::api::EventState value);
    Fields updateEventClassRelatedParams();

    void toggleActionUseSource();

private:
    nx::Uuid m_id;
    bool m_modified;

    nx::vms::api::EventType m_eventType;
    QSet<nx::Uuid> m_eventResources;
    nx::vms::event::EventParameters m_eventParams;
    nx::vms::api::EventState m_eventState;
    QHash<nx::vms::api::EventType, nx::vms::event::EventParameters> m_cachedEventParams;

    nx::vms::api::ActionType m_actionType;
    QSet<nx::Uuid> m_actionResources;
    nx::vms::event::ActionParameters m_actionParams;
    QHash<nx::vms::api::ActionType, nx::vms::event::ActionParameters> m_cachedActionParams;

    int m_aggregationPeriodSec;
    bool m_disabled;
    QString m_comments;
    QString m_schedule;

    QStandardItemModel *m_eventTypesModel;
    QStandardItemModel *m_eventStatesModel;
    QStandardItemModel *m_actionTypesModel;

    std::unique_ptr<nx::vms::event::StringsHelper> m_helper;
};

typedef QSharedPointer<QnBusinessRuleViewModel> QnBusinessRuleViewModelPtr;

Q_DECLARE_OPERATORS_FOR_FLAGS(QnBusinessRuleViewModel::Fields)
