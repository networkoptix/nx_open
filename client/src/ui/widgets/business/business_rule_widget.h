#ifndef BUSINESS_RULE_WIDGET_H
#define BUSINESS_RULE_WIDGET_H

#include <QtWidgets/QWidget>

#include <business/business_fwd.h>
#include <business/business_event_rule.h>
#include <business/events/abstract_business_event.h>

#include <ui/models/business_rules_view_model.h>

#include <ui/widgets/business/abstract_business_params_widget.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
    class BusinessRuleWidget;
}

class QStateMachine;
class QStandardItemModel;

class QnBusinessRuleWidget : public QWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    
    typedef QWidget base_type;
public:
    explicit QnBusinessRuleWidget(QWidget *parent = 0, QnWorkbenchContext *context = NULL);
    virtual ~QnBusinessRuleWidget();

    QnBusinessRuleViewModel* model() const;
    void setModel(QnBusinessRuleViewModel* model);

public slots:
    void at_scheduleButton_clicked();

protected:
    /**
     * @brief initEventParameters   Display widget with current event paramenters.
     */
    void initEventParameters();

    void initActionParameters();

    virtual bool eventFilter(QObject *object, QEvent *event) override;

private slots:
    void at_model_dataChanged(QnBusinessRuleViewModel* model, QnBusiness::Fields fields);

    void at_eventTypeComboBox_currentIndexChanged(int index);
    void at_eventStatesComboBox_currentIndexChanged(int index);
    void at_actionTypeComboBox_currentIndexChanged(int index);
    void at_commentsLineEdit_textChanged(const QString &value);

    void at_eventResourcesHolder_clicked();
    void at_actionResourcesHolder_clicked();

    void updateModelAggregationPeriod();
private:
    QScopedPointer<Ui::BusinessRuleWidget> ui;

    QnBusinessRuleViewModel* m_model;

    QnAbstractBusinessParamsWidget *m_eventParameters;
    QnAbstractBusinessParamsWidget *m_actionParameters;

    QMap<QnBusiness::EventType, QnAbstractBusinessParamsWidget*> m_eventWidgetsByType;
    QMap<QnBusiness::ActionType, QnAbstractBusinessParamsWidget*> m_actionWidgetsByType;

    QnResourceList m_dropResources;

    bool m_updating;
};

#endif // BUSINESS_RULE_WIDGET_H
