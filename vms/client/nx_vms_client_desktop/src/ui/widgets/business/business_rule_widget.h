#pragma once

#include <QtWidgets/QWidget>

#include <nx/vms/event/event_fwd.h>
#include <nx/vms/event/rule.h>
#include <nx/vms/event/events/abstract_event.h>

#include <ui/models/business_rules_view_model.h>

#include <ui/widgets/business/abstract_business_params_widget.h>
#include <nx/vms/client/desktop/common/widgets/panel.h>
#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

class QStateMachine;
class QStandardItemModel;

namespace nx::vms::client::desktop {

class MimeData;
class Aligner;

} // namespace nx::vms::client::desktop

namespace Ui { class BusinessRuleWidget; }

class QnBusinessRuleWidget:
    public Connective<nx::vms::client::desktop::Panel>,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef Connective<nx::vms::client::desktop::Panel> base_type;

public:
    explicit QnBusinessRuleWidget(QWidget *parent = 0);
    virtual ~QnBusinessRuleWidget();

    QnBusinessRuleViewModelPtr model() const;
    void setModel(const QnBusinessRuleViewModelPtr &model);

public slots:
    void at_scheduleButton_clicked();

protected:
    /**
     * @brief initEventParameters   Display widget with current event parameters.
     */
    void initEventParameters();

    void initActionParameters();

    virtual bool eventFilter(QObject *object, QEvent *event) override;

private:
    using Field = QnBusinessRuleViewModel::Field;
    using Fields = QnBusinessRuleViewModel::Fields;
    using Column = QnBusinessRuleViewModel::Column;

private slots:
    void at_model_dataChanged(Fields fields);

    void at_eventTypeComboBox_currentIndexChanged(int index);
    void at_eventStatesComboBox_currentIndexChanged(int index);
    void at_actionTypeComboBox_currentIndexChanged(int index);
    void at_commentsLineEdit_textChanged(const QString &value);

    void at_eventResourcesHolder_clicked();
    void at_actionResourcesHolder_clicked();

    void updateModelAggregationPeriod();

    void updateWarningLabel();

private:
    void retranslateUi();

private:
    QScopedPointer<Ui::BusinessRuleWidget> ui;

    QnBusinessRuleViewModelPtr m_model;

    QnAbstractBusinessParamsWidget *m_eventParameters;
    QnAbstractBusinessParamsWidget *m_actionParameters;

    QMap<nx::vms::api::EventType, QnAbstractBusinessParamsWidget*> m_eventWidgetsByType;
    QMap<nx::vms::api::ActionType, QnAbstractBusinessParamsWidget*> m_actionWidgetsByType;

    std::unique_ptr<nx::vms::client::desktop::MimeData> m_mimeData;

    bool m_updating;

    nx::vms::client::desktop::Aligner* const m_eventAligner;
    nx::vms::client::desktop::Aligner* const m_actionAligner;
};
