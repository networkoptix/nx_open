#pragma once

#include <QtCore/QVariant>

#include <QtGui/QStandardItemModel>

#include <business/business_event_rule.h>
#include <business/business_fwd.h>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include <nx/utils/uuid.h>

namespace QnBusiness {
    enum Columns {
        ModifiedColumn,
        DisabledColumn,
        EventColumn,
        SourceColumn,
        SpacerColumn,
        ActionColumn,
        TargetColumn,
        AggregationColumn
    };

    QList<Columns> allColumns();

    enum Field {
        ModifiedField           = 0x00000001,
        EventTypeField          = 0x00000002,
        EventResourcesField     = 0x00000004,
        EventParamsField        = 0x00000008,
        EventStateField         = 0x00000010,
        ActionTypeField         = 0x00000020,
        ActionResourcesField    = 0x00000040,
        ActionParamsField       = 0x00000080,
        AggregationField        = 0x00000100,
        DisabledField           = 0x00000200,
        CommentsField           = 0x00000400,
        ScheduleField           = 0x00000800,
        AllFieldsMask           = 0x0000FFFF
    };
    Q_DECLARE_FLAGS(Fields, Field)

}

Q_DECLARE_OPERATORS_FOR_FLAGS(QnBusiness::Fields)

typedef QVector<QnUuid> IDList;
class QnBusinessStringsHelper;

class QnBusinessRuleViewModel: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef QObject base_type;

public:
    QnBusinessRuleViewModel(QObject *parent = 0);
    virtual ~QnBusinessRuleViewModel();

    QVariant data(const int column, const int role = Qt::DisplayRole) const;
    bool setData(const int column, const QVariant &value, int role);

    void loadFromRule(QnBusinessEventRulePtr businessRule);
    QnBusinessEventRulePtr createRule() const;

    QString getText(const int column, const bool detailed = true) const;
    QString getToolTip(const int column) const;
    QIcon getIcon(const int column) const;
    int getHelpTopic(const int column) const;

    bool isValid(int column) const;
    bool isValid() const; //checks validity for all row

    QnUuid id() const;

    bool isModified() const;
    void setModified(bool value);

    QnBusiness::EventType eventType() const;
    void setEventType(const QnBusiness::EventType value);

    QSet<QnUuid> eventResources() const;
    void setEventResources(const QSet<QnUuid>& value);

    QnBusinessEventParameters eventParams() const;
    void setEventParams(const QnBusinessEventParameters& params);

    QnBusiness::EventState eventState() const;
    void setEventState(QnBusiness::EventState state);

    QnBusiness::ActionType actionType() const;
    void setActionType(const QnBusiness::ActionType value);

    QSet<QnUuid> actionResources() const;
    void setActionResources(const QSet<QnUuid>& value);

    bool isActionProlonged() const;

    QnBusinessActionParameters actionParams() const;
    void setActionParams(const QnBusinessActionParameters& params);

    int aggregationPeriod() const;
    void setAggregationPeriod(int secs);

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

signals:
    void dataChanged(QnBusiness::Fields fields);

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

    static QString toggleStateToModelString(QnBusiness::EventState value);
private:
    QnUuid m_id;
    bool m_modified;

    QnBusiness::EventType m_eventType;
    QSet<QnUuid> m_eventResources;
    QnBusinessEventParameters m_eventParams;
    QnBusiness::EventState m_eventState;

    QnBusiness::ActionType m_actionType;
    QSet<QnUuid> m_actionResources;
    QnBusinessActionParameters m_actionParams;

    int m_aggregationPeriodSec;
    bool m_disabled;
    QString m_comments;
    QString m_schedule;

    QStandardItemModel *m_eventTypesModel;
    QStandardItemModel *m_eventStatesModel;
    QStandardItemModel *m_actionTypesModel;

    std::unique_ptr<QnBusinessStringsHelper> m_helper;
};

typedef QSharedPointer<QnBusinessRuleViewModel> QnBusinessRuleViewModelPtr;
