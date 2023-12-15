// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "business_rule_widget.h"
#include "ui_business_rule_widget.h"

#include <QtCore/QParallelAnimationGroup>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QScopedValueRollback>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QIcon>
#include <QtGui/QStandardItem>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QCheckBox>

#include <business/business_resource_validation.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/resource_dialogs/camera_selection_dialog.h>
#include <nx/vms/client/desktop/resource_dialogs/server_selection_dialog.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/dialogs/week_time_schedule_dialog.h>
#include <nx/vms/client/desktop/ui/event_rules/subject_selection_dialog.h>
#include <nx/vms/client/desktop/utils/mime_data.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/system_settings.h>
#include <ui/delegates/resource_selection_dialog_delegate.h>
#include <ui/widgets/business/aggregation_widget.h>
#include <ui/widgets/business/business_action_widget_factory.h>
#include <ui/widgets/business/business_event_widget_factory.h>
#include <ui/workbench/workbench_context.h>

using namespace nx;
using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;
using namespace nx::vms::common;

using nx::vms::api::EventType;
using nx::vms::api::ActionType;

namespace {

template<typename Policy>
void updateEventCameras(
    const QnBusinessRuleViewModelPtr model,
    QWidget* parent)
{
    QnUuidSet selectedCameras = model->eventResources();
    if (CameraSelectionDialog::selectCameras<Policy>(selectedCameras, parent))
        model->setEventResources(selectedCameras);
}

QIcon iconHelper(QIcon base)
{
    if (base.isNull())
        return base;

    return QIcon(nx::vms::client::core::Skin::maximumSizePixmap(
        base, QIcon::Selected, QIcon::Off, false));
}

void setActionResourcesHolderDisplayFromModel(
    QPushButton* resourcesHolder,
    const QnBusinessRuleViewModel* model)
{
    using Column = QnBusinessRuleViewModel::Column;
    resourcesHolder->setText(model->data(Column::target, Qn::ShortTextRole).toString());
    resourcesHolder->setIcon(
        iconHelper(model->data(Column::target, Qt::DecorationRole).value<QIcon>()));
}

static const QColor klight16Color = "#698796";
static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kIconSubstitutions = {
    {QnIcon::Normal, {{klight16Color, "light4"}}},
};

} // namespace

QnBusinessRuleWidget::QnBusinessRuleWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::BusinessRuleWidget),
    m_model(),
    m_eventParameters(nullptr),
    m_actionParameters(nullptr),
    m_updating(false),
    m_eventAligner(new Aligner(this)),
    m_actionAligner(new Aligner(this))
{
    ui->setupUi(this);

    ui->scheduleButton->setIcon(qnSkin->icon(lit("buttons/schedule_20.svg"), kIconSubstitutions));
    setHelpTopic(ui->scheduleButton, HelpTopic::Id::EventsActions_Schedule);

    ui->eventDefinitionGroupBox->installEventFilter(this);
    ui->actionDefinitionGroupBox->installEventFilter(this);

    connect(ui->eventTypeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(at_eventTypeComboBox_currentIndexChanged(int)));
    connect(ui->eventStatesComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(at_eventStatesComboBox_currentIndexChanged(int)));
    connect(ui->eventResourcesHolder, SIGNAL(clicked()), this, SLOT(at_eventResourcesHolder_clicked()));
    connect(ui->actionResourcesHolder, SIGNAL(clicked()), this, SLOT(at_actionResourcesHolder_clicked()));
    connect(ui->scheduleButton, SIGNAL(clicked()), this, SLOT(at_scheduleButton_clicked()));

    connect(ui->actionTypeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(at_actionTypeComboBox_currentIndexChanged(int)));

    connect(ui->aggregationWidget, SIGNAL(valueChanged()), this, SLOT(updateModelAggregationPeriod()));

    connect(ui->commentsLineEdit, SIGNAL(textChanged(QString)), this, SLOT(at_commentsLineEdit_textChanged(QString)));

    // Checkbox notifies model about click, model holds action for it and value for it. Should
    // be replaced with some kind of read-only check box eventually.
    connect(ui->useEventSourceServerCheckBox, &QCheckBox::clicked, this,
        [this](bool checked) { m_model->toggleActionUseSourceServer(); });
    connect(ui->useEventSourceCameraCheckBox, &QCheckBox::clicked, this,
        [this](bool checked) { m_model->toggleActionUseSourceCamera(); });

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
            return '<' + source + '>';
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
        disconnect(m_model.get(), nullptr, this, nullptr);

    m_model = model;

    if (!m_model)
        return;

    {
        QScopedValueRollback<bool> guard(m_updating, true);
        ui->eventTypeComboBox->setModel(m_model->eventTypesModel());
        ui->eventStatesComboBox->setModel(m_model->eventStatesModel());
        ui->actionTypeComboBox->setModel(m_model->actionTypesModel());
    }

    connect(m_model.get(), &QnBusinessRuleViewModel::dataChanged, this, &QnBusinessRuleWidget::at_model_dataChanged);
    at_model_dataChanged(Field::all);
}

void QnBusinessRuleWidget::at_model_dataChanged(Fields fields)
{
    if (!m_model || m_updating)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);

    if (fields & Field::eventType)
    {
        QModelIndexList eventTypeIdx = m_model->eventTypesModel()->match(
            m_model->eventTypesModel()->index(0, 0), Qt::UserRole + 1, (int)m_model->eventType(), 1, Qt::MatchExactly);
        ui->eventTypeComboBox->setCurrentIndex(eventTypeIdx.isEmpty() ? 0 : eventTypeIdx.first().row());

        bool isResourceRequired = vms::event::isResourceRequired(m_model->eventType());
        ui->eventResourcesWidget->setVisible(isResourceRequired);

        initEventParameters();
    }

    if (fields & Field::eventState)
    {
        QModelIndexList stateIdx = m_model->eventStatesModel()->match(
            m_model->eventStatesModel()->index(0, 0), Qt::UserRole + 1, (int)m_model->eventState(), 1, Qt::MatchExactly);
        ui->eventStatesComboBox->setCurrentIndex(stateIdx.isEmpty() ? 0 : stateIdx.first().row());
    }

    if (fields & Field::eventResources || fields & Field::eventType)
    {
        ui->eventResourcesHolder->setText(m_model->data(Column::source).toString());
        ui->eventResourcesHolder->setIcon(iconHelper(m_model->data(Column::source,
            Qt::DecorationRole).value<QIcon>()));

        if (vms::event::requiresServerResource(m_model->actionType())
            || nx::vms::event::canUseSourceCamera(m_model->actionType()))
        {
            setActionResourcesHolderDisplayFromModel(ui->actionResourcesHolder, m_model.get());
        }
    }

    if (fields & Field::actionType)
    {
        QModelIndexList actionTypeIdx = m_model->actionTypesModel()->match(m_model->actionTypesModel()->index(0, 0), Qt::UserRole + 1, (int)m_model->actionType(), 1, Qt::MatchExactly);
        ui->actionTypeComboBox->setCurrentIndex(actionTypeIdx.isEmpty() ? 0 : actionTypeIdx.first().row());

        const auto actionType = m_model->actionType();
        const bool isResourceRequired =
            actionType != vms::event::ActionType::fullscreenCameraAction
                && (vms::event::requiresCameraResource(actionType)
                    || vms::event::requiresUserResource(actionType)
                    || vms::event::requiresServerResource(actionType));
        ui->actionResourcesWidget->setVisible(isResourceRequired);

        QString actionAtLabelText;
        switch (m_model->actionType())
        {
            case ActionType::sendMailAction:
                //: "to" is from the sentence "Send email _to_:"
                actionAtLabelText = tr("to");
                break;
            case ActionType::showOnAlarmLayoutAction:
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

        const bool aggregationIsVisible = (m_model->actionType() == ActionType::pushNotificationAction)
            ? !systemSettings()->cloudSystemId().isEmpty()
            : vms::event::allowsAggregation(m_model->actionType());
        ui->aggregationWidget->setVisible(aggregationIsVisible);

        // Push notification action widget has non-standard placeholder that is shown when system
        // is not connected to Cloud. When this placeholder is shown, interval of action checkbox
        // should be hidden for better user experience.
        // User may connect to Cloud when the widget is shown, so we need to update visibility.
        if (m_intervalOfActionUpdater)
        {
            disconnect(m_intervalOfActionUpdater.value());
            m_intervalOfActionUpdater.reset();
        }
        if (m_model->actionType() == ActionType::pushNotificationAction
            && systemSettings()->cloudSystemId().isEmpty())
        {
            m_intervalOfActionUpdater.emplace(
                connect(systemSettings(), &SystemSettings::cloudCredentialsChanged, this,
                    [this]()
                    {
                        ui->aggregationWidget->setVisible(
                            !systemSettings()->cloudSystemId().isEmpty());
                    }));
        }

        initActionParameters();
    }

    if (fields & (Field::eventType | Field::actionType | Field::actionParams))
    {
        ui->useEventSourceServerCheckBox->setVisible(
            vms::event::requiresServerResource(m_model->actionType()));

        ui->useEventSourceServerCheckBox->setEnabled(m_model->actionCanUseSourceServer());
        ui->useEventSourceServerCheckBox->setChecked(m_model->actionIsUsingSourceServer());

        const auto canUseSourceCamera = nx::vms::event::canUseSourceCamera(m_model->actionType());
        ui->useEventSourceCameraCheckBox->setVisible(canUseSourceCamera);
        if (canUseSourceCamera)
        {
            ui->useEventSourceCameraCheckBox->setText(m_model->sourceCameraCheckboxText());
            ui->useEventSourceCameraCheckBox->setEnabled(m_model->actionCanUseSourceCamera());
            ui->useEventSourceCameraCheckBox->setChecked(m_model->isActionUsingSourceCamera());
        }

        if (m_model->eventType() == EventType::softwareTriggerEvent)
        {
            /* SoftwareTriggerEvent is prolonged if its action is prolonged. */
            ui->eventStatesComboBox->setVisible(false);
        }
        else
        {
            const bool isEventProlonged = vms::event::hasToggleState(
                m_model->eventType(),
                m_model->eventParams(),
                systemContext());
            ui->eventStatesComboBox->setVisible(isEventProlonged && !m_model->isActionProlonged());
        }
    }

    if (fields & (Field::actionResources | Field::actionType | Field::actionParams))
    {
        setActionResourcesHolderDisplayFromModel(ui->actionResourcesHolder, m_model.get());
    }

    if (fields & Field::aggregation)
    {
        ui->aggregationWidget->setValue(m_model->aggregationPeriod());
    }

    if (fields & Field::comments)
    {
        QString text = m_model->comments();
        if (ui->commentsLineEdit->text() != text)
            ui->commentsLineEdit->setText(text);
    }

    updateWarningLabel();
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
        m_eventParameters = QnBusinessEventWidgetFactory::createWidget(
            m_model->eventType(),
            systemContext(),
            this);
        m_eventWidgetsByType[m_model->eventType()] = m_eventParameters;
    }
    if (m_eventParameters)
    {
        ui->eventParamsLayout->addWidget(m_eventParameters);
        m_eventParameters->updateTabOrder(ui->eventResourcesHolder, ui->scheduleButton);
        m_eventParameters->setVisible(true);
        m_eventParameters->setModel(m_model);

        if (const auto aligner = m_eventParameters->findChild<Aligner*>())
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
        m_actionParameters = QnBusinessActionWidgetFactory::createWidget(
            m_model->actionType(),
            context(),
            this);
        m_actionWidgetsByType[m_model->actionType()] = m_actionParameters;
    }

    const auto getTabBeforeTarget = [this]() -> QWidget *
    {
        const bool aggregationIsVisible = (m_model->actionType() == ActionType::pushNotificationAction)
            ? !systemSettings()->cloudSystemId().isEmpty()
            : vms::event::allowsAggregation(m_model->actionType());
        if (aggregationIsVisible)
            return ui->aggregationWidget->lastTabItem();

        const auto actionType = m_model->actionType();
        const bool resourceIsVisible =
            actionType != vms::event::ActionType::fullscreenCameraAction
                && (vms::event::requiresCameraResource(actionType)
                    || vms::event::requiresUserResource(actionType));
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

        if (const auto aligner = m_actionParameters->findChild<Aligner*>())
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

        m_mimeData.reset(new MimeData{de->mimeData()});
        if (m_mimeData->allowedInWindowContext(workbench()->windowContext())
            && !m_mimeData->resources().empty())
        {
            de->acceptProposedAction();
        }

        return true;
    }
    else if (event->type() == QEvent::Drop)
    {
        QDropEvent* de = static_cast<QDropEvent*>(event);
        if (m_mimeData->allowedInWindowContext(workbench()->windowContext())
            && !m_mimeData->resources().empty())
        {
            if (object == ui->eventDefinitionGroupBox)
            {
                auto resources = m_model->eventResources();
                for (const auto &res: m_mimeData->resources())
                    resources << res->getId();
                m_model->setEventResources(resources);
            }
            else if (object == ui->actionDefinitionGroupBox)
            {
                auto resources = m_model->actionResources();
                for (const auto& res: m_mimeData->resources())
                    resources << res->getId();
                m_model->setActionResources(resources);
            }
            m_mimeData.reset();
            de->acceptProposedAction();
        }
        return true;
    }
    else if (event->type() == QEvent::DragLeave)
    {
        m_mimeData.reset();
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
    vms::api::EventType val = (vms::api::EventType)typeIdx;
    m_model->setEventType(val);
}

void QnBusinessRuleWidget::at_eventStatesComboBox_currentIndexChanged(int index)
{
    if (!m_model || m_updating || index == -1)
        return;

    if (!vms::event::hasToggleState(m_model->eventType(), m_model->eventParams(), systemContext())
        || m_model->isActionProlonged())
    {
        return;
    }

    int typeIdx = m_model->eventStatesModel()->item(index)->data().toInt();
    vms::api::EventState val = (vms::api::EventState) typeIdx;
    m_model->setEventState(val);
}

void QnBusinessRuleWidget::at_actionTypeComboBox_currentIndexChanged(int index)
{
    if (!m_model || m_updating)
        return;

    int typeIdx = m_model->actionTypesModel()->item(index)->data().toInt();
    vms::api::ActionType val = (vms::api::ActionType)typeIdx;
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

    vms::api::EventType eventType = m_model->eventType();

    if (vms::event::requiresCameraResource(eventType))
    {
        if (eventType == EventType::cameraMotionEvent)
            updateEventCameras<QnCameraMotionPolicy>(m_model, this);
        else if (eventType == EventType::cameraInputEvent)
            updateEventCameras<QnCameraInputPolicy>(m_model, this);
        else if (eventType == EventType::analyticsSdkEvent)
            updateEventCameras<QnCameraAnalyticsPolicy>(m_model, this);
        else if (eventType == EventType::analyticsSdkObjectDetected)
            updateEventCameras<QnCameraAnalyticsPolicy>(m_model, this);
        else
            updateEventCameras<CameraSelectionDialog::DummyPolicy>(m_model, this);
    }
    else if (vms::event::requiresServerResource(eventType))
    {
        bool dialogAccepted = false;
        QnUuidSet selectedServers = m_model->eventResources();

        switch (eventType)
        {
        case EventType::poeOverBudgetEvent:
            dialogAccepted = ServerSelectionDialog::selectServers(selectedServers,
                QnPoeOverBudgetPolicy::isServerValid, QnPoeOverBudgetPolicy::infoText(), this);
            break;
        case EventType::fanErrorEvent:
            dialogAccepted = ServerSelectionDialog::selectServers(selectedServers,
                QnFanErrorPolicy::isServerValid, QnFanErrorPolicy::infoText(), this);
            break;
        default:
            dialogAccepted = ServerSelectionDialog::selectServers(selectedServers,
                ServerSelectionDialog::ServerFilter(), QString(), this);
            break;
        }

        m_model->setEventResources(selectedServers);
    }
}

void QnBusinessRuleWidget::at_actionResourcesHolder_clicked()
{
    if (!m_model)
        return;

    if (vms::event::requiresUserResource(m_model->actionType()))
    {
        SubjectSelectionDialog dialog(this);
        dialog.setCheckedSubjects(m_model->actionResources());

        auto params = m_model->actionParams();
        dialog.setAllUsers(params.allUsers);

        if (m_model->actionType() == ActionType::sendMailAction)
        {
            QSharedPointer<QnSendEmailActionDelegate> dialogDelegate(
                new QnSendEmailActionDelegate(this));

            dialog.setAllUsersSelectorEnabled(false); //< No spam, baby.

            dialog.setUserValidator(
                [dialogDelegate](const QnUserResourcePtr& user)
                {
                    // TODO: #vkutin This adapter is rather sub-optimal.
                    return dialogDelegate->isValid(user->getId());
                });

            const auto updateAlert =
                [&dialog, dialogDelegate]
                {
                    // TODO: #vkutin #3.2 Full updates like this are slow. Refactor in 3.2.
                    dialog.showAlert(dialogDelegate->validationMessage(
                        dialog.checkedSubjects()));
                };

            connect(&dialog, &SubjectSelectionDialog::changed, this, updateAlert);
            updateAlert();
        }

        if (dialog.exec() != QDialog::Accepted)
            return;

        m_model->setActionResources(dialog.checkedSubjects());

        params.allUsers = dialog.allUsers();
        m_model->setActionParams(params);
    }
    else if (vms::event::requiresCameraResource(m_model->actionType()))
    {
        bool dialogAccepted = false;
        QnUuidSet selectedCameras = m_model->actionResources();

        switch (m_model->actionType())
        {
            case ActionType::cameraRecordingAction:
                dialogAccepted = CameraSelectionDialog::selectCameras<QnCameraRecordingPolicy>(
                    selectedCameras, this);
                break;
            case ActionType::bookmarkAction:
                dialogAccepted = CameraSelectionDialog::selectCameras<QnBookmarkActionPolicy>(
                    selectedCameras, this);
                break;
            case ActionType::cameraOutputAction:
                dialogAccepted = CameraSelectionDialog::selectCameras<QnCameraOutputPolicy>(
                    selectedCameras, this);
                break;
            case ActionType::executePtzPresetAction:
                dialogAccepted = CameraSelectionDialog::selectCameras<QnExecPtzPresetPolicy>(
                    selectedCameras, this);
                break;
            case ActionType::playSoundAction:
            case ActionType::playSoundOnceAction:
            case ActionType::sayTextAction:
                dialogAccepted = CameraSelectionDialog::selectCameras<QnCameraAudioTransmitPolicy>(
                    selectedCameras, this);
                break;
            default:
                dialogAccepted = CameraSelectionDialog::selectCameras
                    <CameraSelectionDialog::DummyPolicy>(selectedCameras, this);
                break;
        }

        if (dialogAccepted)
            m_model->setActionResources(selectedCameras);
    }
    else if (vms::event::requiresServerResource(m_model->actionType()))
    {
        bool dialogAccepted = false;
        QnUuidSet selectedServers = m_model->actionResources();

        switch (m_model->actionType())
        {
            case ActionType::buzzerAction:
                dialogAccepted = ServerSelectionDialog::selectServers(selectedServers,
                    QnBuzzerPolicy::isServerValid, QnBuzzerPolicy::infoText(), this);
                break;
            default:
                dialogAccepted = ServerSelectionDialog::selectServers(selectedServers,
                    ServerSelectionDialog::ServerFilter(), QString(), this);
                break;
        }

        if (dialogAccepted)
            m_model->setActionResources(selectedServers);
    }
}

void QnBusinessRuleWidget::at_scheduleButton_clicked()
{
    if (!m_model)
        return;

    QScopedPointer<WeekTimeScheduleDialog> dialog(new WeekTimeScheduleDialog(this));
    dialog->setScheduleTasks(m_model->schedule());
    if (!dialog->exec())
        return;
    m_model->setSchedule(dialog->scheduleTasks());
}

void QnBusinessRuleWidget::updateWarningLabel()
{
    static const QString kGenericEventWarning = tr(
        "Force Acknowledgement will only work for Generic Events"
        " if camera identifiers are used in the Generic Event URL");

    static const QString kCameraPreRecordingWarning = tr(
        "High pre-recording time will increase RAM utilization on the server");

    using namespace std::chrono;
    static constexpr auto kPreRecordingAlertThreshold = 30s; //< Same as for Camera Schedule.

    QString warning;

    if (m_model->eventType() == EventType::userDefinedEvent
        && m_model->actionType() == ActionType::showPopupAction
        && m_model->actionParams().needConfirmation)
    {
        warning = kGenericEventWarning;
    }
    if (m_model->actionType() == ActionType::cameraRecordingAction
        && milliseconds(m_model->actionParams().recordBeforeMs) > kPreRecordingAlertThreshold)
    {
        warning = kCameraPreRecordingWarning;
    }

    ui->actionWarningLabel->setText(warning);
    ui->actionWarningLabel->setVisible(!warning.isEmpty());
}
