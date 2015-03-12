#include "business_rule_widget.h"
#include "ui_business_rule_widget.h"

#include <QtCore/QStateMachine>
#include <QtCore/QState>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QParallelAnimationGroup>

#include <QtGui/QStandardItemModel>
#include <QtGui/QStandardItem>
#include <QtWidgets/QMessageBox>
#include <QtGui/QIcon>
#include <QtGui/QDragEnterEvent>

#include <business/business_resource_validation.h>
//#include <business/events/motion_business_event.h>
//#include <business/events/camera_input_business_event.h>
//#include <business/actions/recording_business_action.h>
//#include <business/actions/camera_output_business_action.h>
//#include <business/actions/sendmail_business_action.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/common/palette.h>
#include <ui/dialogs/resource_selection_dialog.h>
#include <ui/dialogs/week_time_schedule_dialog.h>
#include <ui/delegates/resource_selection_dialog_delegate.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/widgets/business/aggregation_widget.h>
#include <ui/widgets/business/business_event_widget_factory.h>
#include <ui/widgets/business/business_action_widget_factory.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_resource.h>

#include <client/client_settings.h>
#include <utils/common/scoped_value_rollback.h>

namespace {
    QString toggleStateToString(QnBusiness::EventState value, bool prolonged) {
        switch( value )
        {
            case QnBusiness::InactiveState:
                return QObject::tr("Stops");
            case QnBusiness::ActiveState:
                return QObject::tr("Starts");
            case QnBusiness::UndefinedState:
            if (prolonged)
                return QObject::tr("Starts/Stops");
            else
                return QObject::tr("Occurs");
        }
        return QString();
    }

} // namespace

QnBusinessRuleWidget::QnBusinessRuleWidget(QWidget *parent, QnWorkbenchContext *context) :
    base_type(parent),
    QnWorkbenchContextAware(parent, context),
    ui(new Ui::BusinessRuleWidget),
    m_model(NULL),
    m_eventParameters(NULL),
    m_actionParameters(NULL),
    m_updating(false)
{
    ui->setupUi(this);

    setHelpTopic(ui->scheduleButton, Qn::EventsActions_Schedule_Help);

    ui->eventDefinitionGroupBox->installEventFilter(this);
    ui->actionDefinitionGroupBox->installEventFilter(this);

    setPaletteColor(this, QPalette::Window, palette().color(QPalette::Window).lighter());

    connect(ui->eventTypeComboBox,          SIGNAL(currentIndexChanged(int)),   this, SLOT(at_eventTypeComboBox_currentIndexChanged(int)));
    connect(ui->eventStatesComboBox,        SIGNAL(currentIndexChanged(int)),   this, SLOT(at_eventStatesComboBox_currentIndexChanged(int)));
    connect(ui->eventResourcesHolder,       SIGNAL(clicked()),                  this, SLOT(at_eventResourcesHolder_clicked()));
    connect(ui->actionResourcesHolder,      SIGNAL(clicked()),                  this, SLOT(at_actionResourcesHolder_clicked()));
    connect(ui->scheduleButton,             SIGNAL(clicked()),                  this, SLOT(at_scheduleButton_clicked()));

    connect(ui->actionTypeComboBox,         SIGNAL(currentIndexChanged(int)),   this, SLOT(at_actionTypeComboBox_currentIndexChanged(int)));

    connect(ui->aggregationWidget,          SIGNAL(valueChanged()),             this, SLOT(updateModelAggregationPeriod()));

    connect(ui->commentsLineEdit,           SIGNAL(textChanged(QString)),       this, SLOT(at_commentsLineEdit_textChanged(QString)));
}

QnBusinessRuleWidget::~QnBusinessRuleWidget()
{
}

QnBusinessRuleViewModel* QnBusinessRuleWidget::model() const {
    return m_model;
}

void QnBusinessRuleWidget::setModel(QnBusinessRuleViewModel *model) {
    if (m_model)
        disconnect(m_model, 0, this, 0);

    m_model = model;

    if (!m_model) {
/*        ui->eventTypeComboBox->setModel(NULL);
        ui->eventStatesComboBox->setModel(NULL);
        ui->actionTypeComboBox->setModel(NULL);*/
        //TODO: #GDM #Business clear model? dummy?
        return;
    }

    {
        QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);
        ui->eventTypeComboBox->setModel(m_model->eventTypesModel());
        ui->eventStatesComboBox->setModel(m_model->eventStatesModel());
        ui->actionTypeComboBox->setModel(m_model->actionTypesModel());
    }
    setEnabled(!m_model->system());

    connect(m_model, SIGNAL(dataChanged(QnBusinessRuleViewModel*, QnBusiness::Fields)),
            this, SLOT(at_model_dataChanged(QnBusinessRuleViewModel*, QnBusiness::Fields)));
    at_model_dataChanged(m_model, QnBusiness::AllFieldsMask);
}

void QnBusinessRuleWidget::at_model_dataChanged(QnBusinessRuleViewModel *model,  QnBusiness::Fields fields) {
    if (!model || m_model != model || m_updating)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    if (fields & QnBusiness::EventTypeField) {

        QModelIndexList eventTypeIdx = m_model->eventTypesModel()->match(
                    m_model->eventTypesModel()->index(0, 0), Qt::UserRole + 1, (int)m_model->eventType(), 1, Qt::MatchExactly);
        ui->eventTypeComboBox->setCurrentIndex(eventTypeIdx.isEmpty() ? 0 : eventTypeIdx.first().row());

        bool isResourceRequired = QnBusiness::isResourceRequired(m_model->eventType());
        ui->eventResourcesWidget->setVisible(isResourceRequired);

        initEventParameters();
    }

    if (fields & QnBusiness::EventStateField) {
        QModelIndexList stateIdx = m_model->eventStatesModel()->match(
                    m_model->eventStatesModel()->index(0, 0), Qt::UserRole + 1, (int)m_model->eventState(), 1, Qt::MatchExactly);
        ui->eventStatesComboBox->setCurrentIndex(stateIdx.isEmpty() ? 0 : stateIdx.first().row());
    }

    if (fields & QnBusiness::EventResourcesField) {
        ui->eventResourcesHolder->setText(m_model->data(QnBusiness::SourceColumn).toString());
        ui->eventResourcesHolder->setIcon(m_model->data(QnBusiness::SourceColumn, Qt::DecorationRole).value<QIcon>());
    }

    if (fields & QnBusiness::ActionTypeField) {
        QModelIndexList actionTypeIdx = m_model->actionTypesModel()->match(m_model->actionTypesModel()->index(0, 0), Qt::UserRole + 1, (int)m_model->actionType(), 1, Qt::MatchExactly);
        ui->actionTypeComboBox->setCurrentIndex(actionTypeIdx.isEmpty() ? 0 : actionTypeIdx.first().row());

        bool isResourceRequired = QnBusiness::requiresCameraResource(m_model->actionType())
                || QnBusiness::requiresUserResource(m_model->actionType());
        ui->actionResourcesWidget->setVisible(isResourceRequired);

        ui->actionAtLabel->setText(m_model->actionType() == QnBusiness::SendMailAction ? tr("to") : tr("at"));

        bool actionIsInstant = !QnBusiness::hasToggleState(m_model->actionType());
        ui->aggregationWidget->setVisible(actionIsInstant);

        initActionParameters();
    }

    if (fields & (QnBusiness::EventTypeField | QnBusiness::ActionTypeField)) {
        bool prolonged = QnBusiness::hasToggleState(m_model->eventType()) && !QnBusiness::hasToggleState(m_model->actionType());
        ui->eventStatesComboBox->setVisible(prolonged);
    }

    if (fields & QnBusiness::ActionResourcesField) {
        ui->actionResourcesHolder->setText(m_model->data(QnBusiness::TargetColumn, Qn::ShortTextRole).toString());
        ui->actionResourcesHolder->setIcon(m_model->data(QnBusiness::TargetColumn, Qt::DecorationRole).value<QIcon>());
    }

    if (fields & QnBusiness::AggregationField) {
        ui->aggregationWidget->setValue(model->aggregationPeriod());
    }

    if (fields & QnBusiness::CommentsField) {
        QString text = model->comments();
        if (ui->commentsLineEdit->text() != text)
            ui->commentsLineEdit->setText(text);
    }
}

void QnBusinessRuleWidget::initEventParameters() {
    if (m_eventParameters) {
        ui->eventParamsLayout->removeWidget(m_eventParameters);
        m_eventParameters->setVisible(false);
        m_eventParameters->setModel(NULL);
    }

    if (!m_model)
        return;

    if (m_eventWidgetsByType.contains(m_model->eventType())) {
        m_eventParameters = m_eventWidgetsByType.find(m_model->eventType()).value();
    } else {
        m_eventParameters = QnBusinessEventWidgetFactory::createWidget(m_model->eventType(), this, context());
        m_eventWidgetsByType[m_model->eventType()] = m_eventParameters;
    }
    if (m_eventParameters) {
        ui->eventParamsLayout->addWidget(m_eventParameters);
        m_eventParameters->updateTabOrder(ui->eventResourcesHolder,  ui->scheduleButton);
        m_eventParameters->setVisible(true);
        m_eventParameters->setModel(m_model);
    } else {
        setTabOrder(ui->eventResourcesHolder, ui->scheduleButton);
    }
}

void QnBusinessRuleWidget::initActionParameters() {
    if (m_actionParameters) {
        ui->actionParamsLayout->removeWidget(m_actionParameters);
        m_actionParameters->setVisible(false);
        m_actionParameters->setModel(NULL);
    }

    if (!m_model)
        return;

    if (m_actionWidgetsByType.contains(m_model->actionType())) {
        m_actionParameters = m_actionWidgetsByType.find(m_model->actionType()).value();
    } else {
        m_actionParameters = QnBusinessActionWidgetFactory::createWidget(m_model->actionType(), this);
        m_actionWidgetsByType[m_model->actionType()] = m_actionParameters;
    }

    if (m_actionParameters) {
        ui->actionParamsLayout->addWidget(m_actionParameters);
        m_actionParameters->updateTabOrder(ui->aggregationWidget,  ui->commentsLineEdit);
        m_actionParameters->setVisible(true);
        m_actionParameters->setModel(m_model);
    } else {
        setTabOrder(ui->aggregationWidget, ui->commentsLineEdit);
    }
}

bool QnBusinessRuleWidget::eventFilter(QObject *object, QEvent *event) {
    if (!m_model)
        return base_type::eventFilter(object, event);;

    if (event->type() == QEvent::DragEnter) {
        QDragEnterEvent* de = static_cast<QDragEnterEvent*>(event);
        m_dropResources = QnWorkbenchResource::deserializeResources(de->mimeData());
        if (!m_dropResources.empty())
            de->acceptProposedAction();
        return true;
    } else if (event->type() == QEvent::Drop) {
        QDropEvent* de = static_cast<QDropEvent*>(event);
        if (!m_dropResources.empty()) {
            if (object == ui->eventDefinitionGroupBox) {
                QnResourceList resources = m_model->eventResources();
                foreach(QnResourcePtr res, m_dropResources) {
                    if (resources.contains(res))
                        continue;
                    resources.append(res);
                }
                m_model->setEventResources(resources);
            } else if (object == ui->actionDefinitionGroupBox) {
                QnResourceList resources = m_model->actionResources();
                foreach(QnResourcePtr res, m_dropResources) {
                    if (resources.contains(res))
                        continue;
                    resources.append(res);
                }
                m_model->setActionResources(resources);
            }
            m_dropResources = QnResourceList();
            de->acceptProposedAction();
        }
        return true;
    } else if (event->type() == QEvent::DragLeave) {
        m_dropResources = QnResourceList();
        return true;
    }

    return base_type::eventFilter(object, event);
}

void QnBusinessRuleWidget::updateModelAggregationPeriod() {
    if (!m_model || m_updating)
        return;
    m_model->setAggregationPeriod(ui->aggregationWidget->value());
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnBusinessRuleWidget::at_eventTypeComboBox_currentIndexChanged(int index) {
    if (!m_model || m_updating)
        return;

    int typeIdx = m_model->eventTypesModel()->item(index)->data().toInt();
    QnBusiness::EventType val = (QnBusiness::EventType)typeIdx;
    m_model->setEventType(val);
}

void QnBusinessRuleWidget::at_eventStatesComboBox_currentIndexChanged(int index) {
    if (!m_model || m_updating)
        return;

    bool prolonged = QnBusiness::hasToggleState(m_model->eventType()) && !QnBusiness::hasToggleState(m_model->actionType());
    if (!prolonged)
        return;

    int typeIdx = m_model->eventStatesModel()->item(index)->data().toInt();
    QnBusiness::EventState val = (QnBusiness::EventState) typeIdx;
    m_model->setEventState(val);
}

void QnBusinessRuleWidget::at_actionTypeComboBox_currentIndexChanged(int index) {
    if (!m_model || m_updating)
        return;

    int typeIdx = m_model->actionTypesModel()->item(index)->data().toInt();
    QnBusiness::ActionType val = (QnBusiness::ActionType)typeIdx;
    m_model->setActionType(val);
}

void QnBusinessRuleWidget::at_commentsLineEdit_textChanged(const QString &value) {
    if (!m_model || m_updating)
        return;

    m_model->setComments(value);
}

void QnBusinessRuleWidget::at_eventResourcesHolder_clicked() {
    if (!m_model)
        return;

    QnResourceSelectionDialog dialog(this); //TODO: #GDM #Business or servers?

    QnBusiness::EventType eventType = m_model->eventType();
    if (eventType == QnBusiness::CameraMotionEvent)
        dialog.setDelegate(new QnCheckResourceAndWarnDelegate<QnCameraMotionPolicy>(this));
    else if (eventType == QnBusiness::CameraInputEvent)
        dialog.setDelegate(new QnCheckResourceAndWarnDelegate<QnCameraInputPolicy>(this));
    dialog.setSelectedResources(m_model->eventResources());

    if (dialog.exec() != QDialog::Accepted)
        return;
    m_model->setEventResources(dialog.selectedResources());
}

void QnBusinessRuleWidget::at_actionResourcesHolder_clicked() {
    if (!m_model)
        return;

    QnResourceSelectionDialog::SelectionTarget target;
    if (QnBusiness::requiresCameraResource(m_model->actionType()))
        target = QnResourceSelectionDialog::CameraResourceTarget;
    else if (QnBusiness::requiresUserResource(m_model->actionType()))
        target = QnResourceSelectionDialog::UserResourceTarget;
    else
        return;

    QnResourceSelectionDialog dialog(target, this);

    QnBusiness::ActionType actionType = m_model->actionType();
    if (actionType == QnBusiness::CameraRecordingAction)
        dialog.setDelegate(new QnCheckResourceAndWarnDelegate<QnCameraRecordingPolicy>(this));
    else if (actionType == QnBusiness::CameraOutputAction || actionType == QnBusiness::CameraOutputOnceAction)
        dialog.setDelegate(new QnCheckResourceAndWarnDelegate<QnCameraOutputPolicy>(this));
    else if (actionType == QnBusiness::SendMailAction)
        dialog.setDelegate(new QnCheckResourceAndWarnDelegate<QnUserEmailPolicy>(this));
    dialog.setSelectedResources(m_model->actionResources());

    if (dialog.exec() != QDialog::Accepted)
        return;
    m_model->setActionResources(dialog.selectedResources());
}

void QnBusinessRuleWidget::at_scheduleButton_clicked() {
    if (!m_model)
        return;

    QScopedPointer<QnWeekTimeScheduleDialog> dialog(new QnWeekTimeScheduleDialog(this));
    dialog->setScheduleTasks(m_model->schedule());
    if (!dialog->exec())
        return;
    m_model->setSchedule(dialog->scheduleTasks());
}


