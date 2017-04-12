#include "business_rule_widget.h"
#include "ui_business_rule_widget.h"

#include <QtCore/QStateMachine>
#include <QtCore/QState>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QParallelAnimationGroup>

#include <QtGui/QStandardItemModel>
#include <QtGui/QStandardItem>
#include <QtGui/QIcon>
#include <QtGui/QDragEnterEvent>

#include <business/business_resource_validation.h>

#include <core/resource/device_dependent_strings.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/common/aligner.h>
#include <ui/common/palette.h>
#include <ui/dialogs/resource_selection_dialog.h>
#include <ui/dialogs/week_time_schedule_dialog.h>
#include <ui/delegates/resource_selection_dialog_delegate.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/skin.h>
#include <ui/widgets/business/aggregation_widget.h>
#include <ui/widgets/business/business_event_widget_factory.h>
#include <ui/widgets/business/business_action_widget_factory.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_resource.h>

#include <client/client_settings.h>
#include <utils/common/scoped_value_rollback.h>

namespace {

QIcon iconHelper(QIcon base)
{
    if (base.isNull())
        return base;

    return QIcon(QnSkin::maximumSizePixmap(base,
        QIcon::Selected, QIcon::Off, false));
}

} // namespace

QnBusinessRuleWidget::QnBusinessRuleWidget(QWidget *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::BusinessRuleWidget),
    m_model(),
    m_eventParameters(NULL),
    m_actionParameters(NULL),
    m_updating(false),
    m_eventAligner(new QnAligner(this)),
    m_actionAligner(new QnAligner(this))
{
    ui->setupUi(this);

    ui->scheduleButton->setIcon(qnSkin->icon(lit("buttons/schedule.png")));
    setHelpTopic(ui->scheduleButton, Qn::EventsActions_Schedule_Help);

    ui->eventDefinitionGroupBox->installEventFilter(this);
    ui->actionDefinitionGroupBox->installEventFilter(this);

    setPaletteColor(this, QPalette::Window, palette().color(QPalette::Window).lighter());

    connect(ui->eventTypeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(at_eventTypeComboBox_currentIndexChanged(int)));
    connect(ui->eventStatesComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(at_eventStatesComboBox_currentIndexChanged(int)));
    connect(ui->eventResourcesHolder, SIGNAL(clicked()), this, SLOT(at_eventResourcesHolder_clicked()));
    connect(ui->actionResourcesHolder, SIGNAL(clicked()), this, SLOT(at_actionResourcesHolder_clicked()));
    connect(ui->scheduleButton, SIGNAL(clicked()), this, SLOT(at_scheduleButton_clicked()));

    connect(ui->actionTypeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(at_actionTypeComboBox_currentIndexChanged(int)));

    connect(ui->aggregationWidget, SIGNAL(valueChanged()), this, SLOT(updateModelAggregationPeriod()));

    connect(ui->commentsLineEdit, SIGNAL(textChanged(QString)), this, SLOT(at_commentsLineEdit_textChanged(QString)));

    m_eventAligner->addWidgets({ ui->eventDoLabel, ui->eventAtLabel });
    m_actionAligner->addWidgets({ ui->actionDoLabel, ui->actionAtLabel });

    retranslateUi();
}

QnBusinessRuleWidget::~QnBusinessRuleWidget()
{}

void QnBusinessRuleWidget::retranslateUi()
{
    auto braced = [](const QString& source)
        {
            return L'<' + source + L'>';
        };

    ui->eventResourcesHolder->setText(braced(QnDeviceDependentStrings::getDefaultNameFromSet(
        resourcePool(),
        tr("Any Device"),
        tr("Any Camera")
    )));

    ui->actionResourcesHolder->setText(QnDeviceDependentStrings::getDefaultNameFromSet(
        resourcePool(),
        tr("Select at least one device"),
        tr("Select at least one camera")
    ));
}


QnBusinessRuleViewModelPtr QnBusinessRuleWidget::model() const
{
    return m_model;
}

void QnBusinessRuleWidget::setModel(const QnBusinessRuleViewModelPtr &model)
{
    if (m_model)
        disconnect(m_model, nullptr, this, nullptr);

    m_model = model;

    if (!m_model)
    {
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

    connect(m_model, &QnBusinessRuleViewModel::dataChanged, this, &QnBusinessRuleWidget::at_model_dataChanged);
    at_model_dataChanged(QnBusiness::AllFieldsMask);
}

void QnBusinessRuleWidget::at_model_dataChanged(QnBusiness::Fields fields)
{
    if (!m_model || m_updating)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    if (fields & QnBusiness::EventTypeField)
    {

        QModelIndexList eventTypeIdx = m_model->eventTypesModel()->match(
            m_model->eventTypesModel()->index(0, 0), Qt::UserRole + 1, (int)m_model->eventType(), 1, Qt::MatchExactly);
        ui->eventTypeComboBox->setCurrentIndex(eventTypeIdx.isEmpty() ? 0 : eventTypeIdx.first().row());

        bool isResourceRequired = QnBusiness::isResourceRequired(m_model->eventType());
        ui->eventResourcesWidget->setVisible(isResourceRequired);

        initEventParameters();
    }

    if (fields & QnBusiness::EventStateField)
    {
        QModelIndexList stateIdx = m_model->eventStatesModel()->match(
            m_model->eventStatesModel()->index(0, 0), Qt::UserRole + 1, (int)m_model->eventState(), 1, Qt::MatchExactly);
        ui->eventStatesComboBox->setCurrentIndex(stateIdx.isEmpty() ? 0 : stateIdx.first().row());
    }

    if (fields & QnBusiness::EventResourcesField)
    {
        ui->eventResourcesHolder->setText(m_model->data(QnBusiness::SourceColumn).toString());
        ui->eventResourcesHolder->setIcon(iconHelper(m_model->data(QnBusiness::SourceColumn,
            Qt::DecorationRole).value<QIcon>()));
    }

    if (fields & QnBusiness::ActionTypeField)
    {
        QModelIndexList actionTypeIdx = m_model->actionTypesModel()->match(m_model->actionTypesModel()->index(0, 0), Qt::UserRole + 1, (int)m_model->actionType(), 1, Qt::MatchExactly);
        ui->actionTypeComboBox->setCurrentIndex(actionTypeIdx.isEmpty() ? 0 : actionTypeIdx.first().row());

        bool isResourceRequired = QnBusiness::requiresCameraResource(m_model->actionType())
            || QnBusiness::requiresUserResource(m_model->actionType());
        ui->actionResourcesWidget->setVisible(isResourceRequired);

        QString actionAtLabelText;
        switch (m_model->actionType())
        {
            case QnBusiness::SendMailAction:
                //: "to" is from the sentence "Send email _to_:"
                actionAtLabelText = tr("to");
                break;
            case QnBusiness::ShowOnAlarmLayoutAction:
                actionAtLabelText = QnDeviceDependentStrings::getDefaultNameFromSet(
                    resourcePool(),
                    tr("Devices"),
                    tr("Cameras"));
                break;
            default:
                //: "at" is from the sentence "Display the text _at_ these cameras"
                actionAtLabelText = tr("at");
                break;
        }
        ui->actionAtLabel->setText(actionAtLabelText);

        ui->aggregationWidget->setVisible(QnBusiness::allowsAggregation(m_model->actionType()));

        initActionParameters();
    }

    if (fields & (QnBusiness::EventTypeField | QnBusiness::ActionTypeField | QnBusiness::ActionParamsField))
    {
        if (m_model->eventType() == QnBusiness::SoftwareTriggerEvent)
        {
            /* SoftwareTriggerEvent is prolonged if its action is prolonged. */
            ui->eventStatesComboBox->setVisible(false);
        }
        else
        {
            const bool isEventProlonged = QnBusiness::hasToggleState(m_model->eventType());
            ui->eventStatesComboBox->setVisible(isEventProlonged && !m_model->isActionProlonged());
        }
    }

    if (fields & (QnBusiness::ActionResourcesField | QnBusiness::ActionTypeField | QnBusiness::ActionParamsField))
    {
        ui->actionResourcesHolder->setText(m_model->data(QnBusiness::TargetColumn, Qn::ShortTextRole).toString());
        ui->actionResourcesHolder->setIcon(iconHelper(m_model->data(QnBusiness::TargetColumn,
            Qt::DecorationRole).value<QIcon>()));
    }

    if (fields & QnBusiness::AggregationField)
    {
        ui->aggregationWidget->setValue(m_model->aggregationPeriod());
    }

    if (fields & QnBusiness::CommentsField)
    {
        QString text = m_model->comments();
        if (ui->commentsLineEdit->text() != text)
            ui->commentsLineEdit->setText(text);
    }
}

void QnBusinessRuleWidget::initEventParameters()
{
    if (m_eventParameters)
    {
        ui->eventParamsLayout->removeWidget(m_eventParameters);
        m_eventParameters->setVisible(false);
        m_eventParameters->setModel(QnBusinessRuleViewModelPtr());
    }

    if (!m_model)
        return;

    if (m_eventWidgetsByType.contains(m_model->eventType()))
    {
        m_eventParameters = m_eventWidgetsByType.find(m_model->eventType()).value();
    }
    else
    {
        m_eventParameters = QnBusinessEventWidgetFactory::createWidget(m_model->eventType(), this, context());
        m_eventWidgetsByType[m_model->eventType()] = m_eventParameters;
    }
    if (m_eventParameters)
    {
        ui->eventParamsLayout->addWidget(m_eventParameters);
        m_eventParameters->updateTabOrder(ui->eventResourcesHolder, ui->scheduleButton);
        m_eventParameters->setVisible(true);
        m_eventParameters->setModel(m_model);

        if (const auto aligner = m_eventParameters->findChild<QnAligner*>())
            m_eventAligner->addAligner(aligner);
    }
    else
    {
        setTabOrder(ui->eventResourcesHolder, ui->scheduleButton);
    }
}

void QnBusinessRuleWidget::initActionParameters()
{
    if (m_actionParameters)
    {
        ui->actionParamsLayout->removeWidget(m_actionParameters);
        m_actionParameters->setVisible(false);
        m_actionParameters->setModel(QnBusinessRuleViewModelPtr());
    }

    if (!m_model)
        return;

    if (m_actionWidgetsByType.contains(m_model->actionType()))
    {
        m_actionParameters = m_actionWidgetsByType.find(m_model->actionType()).value();
    }
    else
    {
        m_actionParameters = QnBusinessActionWidgetFactory::createWidget(m_model->actionType(), this);
        m_actionWidgetsByType[m_model->actionType()] = m_actionParameters;
    }

    const auto getTabBeforeTarget = [this]() -> QWidget *
    {
        if (const bool aggregationIsVisible = QnBusiness::allowsAggregation(m_model->actionType()))
            return ui->aggregationWidget->lastTabItem();

        const bool resourceIsVisible = QnBusiness::requiresCameraResource(m_model->actionType())
            || QnBusiness::requiresUserResource(m_model->actionType());
        if (resourceIsVisible)
            return ui->actionResourcesHolder;

        return ui->actionTypeComboBox;
    };

    if (m_actionParameters)
    {
        ui->actionParamsLayout->addWidget(m_actionParameters);
        m_actionParameters->updateTabOrder(getTabBeforeTarget(), ui->commentsLineEdit);
        m_actionParameters->setVisible(true);
        m_actionParameters->setModel(m_model);

        if (const auto aligner = m_actionParameters->findChild<QnAligner*>())
            m_actionAligner->addAligner(aligner);
    }
    else
    {
        setTabOrder(getTabBeforeTarget(), ui->commentsLineEdit);
    }
}

bool QnBusinessRuleWidget::eventFilter(QObject *object, QEvent *event)
{
    if (!m_model)
        return base_type::eventFilter(object, event);;

    if (event->type() == QEvent::DragEnter)
    {
        QDragEnterEvent* de = static_cast<QDragEnterEvent*>(event);
        m_dropResources = QnWorkbenchResource::deserializeResources(de->mimeData());
        if (!m_dropResources.empty())
            de->acceptProposedAction();
        return true;
    }
    else if (event->type() == QEvent::Drop)
    {
        QDropEvent* de = static_cast<QDropEvent*>(event);
        if (!m_dropResources.empty())
        {
            if (object == ui->eventDefinitionGroupBox)
            {
                auto resources = m_model->eventResources();
                for (const auto &res: m_dropResources)
                    resources << res->getId();
                m_model->setEventResources(resources);
            }
            else if (object == ui->actionDefinitionGroupBox)
            {
                auto resources = m_model->actionResources();
                for (const auto& res: m_dropResources)
                    resources << res->getId();
                m_model->setActionResources(resources);
            }
            m_dropResources = QnResourceList();
            de->acceptProposedAction();
        }
        return true;
    }
    else if (event->type() == QEvent::DragLeave)
    {
        m_dropResources = QnResourceList();
        return true;
    }

    return base_type::eventFilter(object, event);
}

void QnBusinessRuleWidget::updateModelAggregationPeriod()
{
    if (!m_model || m_updating)
        return;
    m_model->setAggregationPeriod(ui->aggregationWidget->value());
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnBusinessRuleWidget::at_eventTypeComboBox_currentIndexChanged(int index)
{
    if (!m_model || m_updating)
        return;

    int typeIdx = m_model->eventTypesModel()->item(index)->data().toInt();
    QnBusiness::EventType val = (QnBusiness::EventType)typeIdx;
    m_model->setEventType(val);
}

void QnBusinessRuleWidget::at_eventStatesComboBox_currentIndexChanged(int index)
{
    if (!m_model || m_updating || index == -1)
        return;

    if (!QnBusiness::hasToggleState(m_model->eventType()) || m_model->isActionProlonged())
        return;

    int typeIdx = m_model->eventStatesModel()->item(index)->data().toInt();
    QnBusiness::EventState val = (QnBusiness::EventState) typeIdx;
    m_model->setEventState(val);
}

void QnBusinessRuleWidget::at_actionTypeComboBox_currentIndexChanged(int index)
{
    if (!m_model || m_updating)
        return;

    int typeIdx = m_model->actionTypesModel()->item(index)->data().toInt();
    QnBusiness::ActionType val = (QnBusiness::ActionType)typeIdx;
    m_model->setActionType(val);
}

void QnBusinessRuleWidget::at_commentsLineEdit_textChanged(const QString &value)
{
    if (!m_model || m_updating)
        return;

    m_model->setComments(value);
}

void QnBusinessRuleWidget::at_eventResourcesHolder_clicked()
{
    if (!m_model)
        return;

    QnResourceSelectionDialog dialog(QnResourceSelectionDialog::Filter::cameras, this); //TODO: #GDM #Business or servers?

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

void QnBusinessRuleWidget::at_actionResourcesHolder_clicked()
{
    if (!m_model)
        return;

    QnResourceSelectionDialog::Filter target;
    if (QnBusiness::requiresCameraResource(m_model->actionType()))
        target = QnResourceSelectionDialog::Filter::cameras;
    else if (QnBusiness::requiresUserResource(m_model->actionType()))
        target = QnResourceSelectionDialog::Filter::users;
    else
        return;

    QnResourceSelectionDialog dialog(target, this);

    QnBusiness::ActionType actionType = m_model->actionType();
    if (actionType == QnBusiness::CameraRecordingAction)
        dialog.setDelegate(new QnCheckResourceAndWarnDelegate<QnCameraRecordingPolicy>(this));
    if (actionType == QnBusiness::BookmarkAction)
        dialog.setDelegate(new QnCheckResourceAndWarnDelegate<QnBookmarkActionPolicy>(this));
    else if (actionType == QnBusiness::CameraOutputAction)
        dialog.setDelegate(new QnCheckResourceAndWarnDelegate<QnCameraOutputPolicy>(this));
    else if (actionType == QnBusiness::ExecutePtzPresetAction)
        dialog.setDelegate(new QnCheckResourceAndWarnDelegate<QnExecPtzPresetPolicy>(this));
    else if (actionType == QnBusiness::SendMailAction)
        dialog.setDelegate(new QnSendEmailActionDelegate(this));
    else if (actionType == QnBusiness::PlaySoundAction || actionType == QnBusiness::PlaySoundOnceAction || actionType == QnBusiness::SayTextAction)
        dialog.setDelegate(new QnCheckResourceAndWarnDelegate<QnCameraAudioTransmitPolicy>(this));

    dialog.setSelectedResources(m_model->actionResources());

    if (dialog.exec() != QDialog::Accepted)
        return;
    m_model->setActionResources(dialog.selectedResources());
}

void QnBusinessRuleWidget::at_scheduleButton_clicked()
{
    if (!m_model)
        return;

    QScopedPointer<QnWeekTimeScheduleDialog> dialog(new QnWeekTimeScheduleDialog(this));
    dialog->setScheduleTasks(m_model->schedule());
    if (!dialog->exec())
        return;
    m_model->setSchedule(dialog->scheduleTasks());
}
