#pragma once

#include <QtCore/QVariant>

#include <QtGui/QStandardItemModel>

#include <nx/vms/event/rule.h>
#include <nx/vms/event/event_fwd.h>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include <nx/utils/uuid.h>

typedef QVector<QnUuid> IDList;
namespace nx { namespace vms { namespace event { class StringsHelper; }}}

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

    void loadFromRule(nx::vms::event::RulePtr businessRule);
    nx::vms::event::RulePtr createRule() const;

    QString getText(Column column, const bool detailed = true) const;
    QString getToolTip(Column column) const;
    QIcon getIcon(Column column) const;
    int getHelpTopic(Column column) const;

    bool isValid(Column column) const;
    bool isValid() const; //checks validity for entire row

    QnUuid id() const;

    bool isModified() const;
    void setModified(bool value);

    nx::vms::event::EventType eventType() const;
    void setEventType(const nx::vms::event::EventType value);

    QSet<QnUuid> eventResources() const;
    void setEventResources(const QSet<QnUuid>& value);

    nx::vms::event::EventParameters eventParams() const;
    void setEventParams(const nx::vms::event::EventParameters& params);

    nx::vms::event::EventState eventState() const;
    void setEventState(nx::vms::event::EventState state);

    nx::vms::event::ActionType actionType() const;
    void setActionType(const nx::vms::event::ActionType value);

    QSet<QnUuid> actionResources() const;
    void setActionResources(const QSet<QnUuid>& value);

    bool isActionProlonged() const;

    nx::vms::event::ActionParameters actionParams() const;
    void setActionParams(const nx::vms::event::ActionParameters& params);

    int aggregationPeriod() const;
    void setAggregationPeriod(int seconds);

    bool disabled() const;
    void setDisabled(const bool value);

    QString schedule() const;

    /**
    * param value binary string encoded as HEX. Each bit represent 1 hour of week schedule. First 24*7 bits is used. Rest of the string is ignored.
    * First day of week is Monday independent of system settings
    */
    void setSchedule(const QString value);

    QString comments() const;
    void setComments(const QString value);

    QStandardItemModel* eventTypesModel();
    QStandardItemModel* eventStatesModel();
    QStandardItemModel* actionTypesModel();

public:
    // TODO: #vkutin #3.2 Temporary workaround to pass "all users" as a special uuid.
    static const QnUuid kAllUsersId;

signals:
    void dataChanged(Fields fields);

private:
    void updateActionTypesModel();
    void updateEventStateModel();

    /**
     * @brief getSourceText     Get text for the Source field.
     * @param detailed          Detailed text is used in the table cell.
     *                          Not detailed - as the button caption and in the advanced view.
     * @return                  Formatted text.
     */
    QString getSourceText(const bool detailed) const;
    QString getTargetText(const bool detailed) const;

    QString getAggregationText() const;

    static QString toggleStateToModelString(nx::vms::event::EventState value);
private:
    QnUuid m_id;
    bool m_modified;

    nx::vms::event::EventType m_eventType;
    QSet<QnUuid> m_eventResources;
    nx::vms::event::EventParameters m_eventParams;
    nx::vms::event::EventState m_eventState;

    nx::vms::event::ActionType m_actionType;
    QSet<QnUuid> m_actionResources;
    nx::vms::event::ActionParameters m_actionParams;

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
