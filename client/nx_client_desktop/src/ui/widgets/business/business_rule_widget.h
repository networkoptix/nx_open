#pragma once

#include <QtWidgets/QWidget>

#include <nx/vms/event/event_fwd.h>
#include <nx/vms/event/rule.h>
#include <nx/vms/event/events/abstract_event.h>

#include <ui/models/business_rules_view_model.h>

#include <ui/widgets/business/abstract_business_params_widget.h>
#include <ui/widgets/common/panel.h>
#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

namespace Ui {
    class BusinessRuleWidget;
}

class QStateMachine;
class QStandardItemModel;
class QnAligner;

namespace nx {
namespace client {
namespace desktop {

class MimeData;

} // namespace desktop
} // namespace client
} // namespace nx

class QnBusinessRuleWidget : public Connective<QnPanel>, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef Connective<QnPanel> base_type;

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

private:
    void retranslateUi();

private:
    QScopedPointer<Ui::BusinessRuleWidget> ui;

    QnBusinessRuleViewModelPtr m_model;

    QnAbstractBusinessParamsWidget *m_eventParameters;
    QnAbstractBusinessParamsWidget *m_actionParameters;

    QMap<nx::vms::event::EventType, QnAbstractBusinessParamsWidget*> m_eventWidgetsByType;
    QMap<nx::vms::event::ActionType, QnAbstractBusinessParamsWidget*> m_actionWidgetsByType;

    std::unique_ptr<nx::client::desktop::MimeData> m_mimeData;

    bool m_updating;

    QnAligner* const m_eventAligner;
    QnAligner* const m_actionAligner;
};
