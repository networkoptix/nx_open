#include "general_system_administration_widget.h"
#include "ui_general_system_administration_widget.h"

#include <api/runtime_info_manager.h>

#include <common/common_module.h>

#include <core/resource/resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource_management/resource_pool.h>

#include <nx_ec/data/api_runtime_data.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_parameters.h>
#include <ui/actions/action_manager.h>
#include <ui/common/custom_painted.h>
#include <ui/common/read_only.h>
#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/style/helper.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_context.h>

#ifdef SYSTEM_NAME_EDITING_ENABLED
#include <utils/common/event_processors.h>
#endif

namespace {

const int kSystemNameFontSizePixels = 24;
const int kSystemNameFontWeight = QFont::DemiBold;

const int kPreferencesButtonSize = 104;
const QMargins kPreferencesButtonMargins(8, 4, 8, 4);

} // unnamed namespace

QnGeneralSystemAdministrationWidget::QnGeneralSystemAdministrationWidget(QWidget *parent /* = nullptr*/) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::GeneralSystemAdministrationWidget)
{
    ui->setupUi(this);

    QFont font;
    font.setPixelSize(kSystemNameFontSizePixels);
    font.setWeight(kSystemNameFontWeight);
    ui->systemNameLabel->setFont(font);
    ui->systemNameLabel->setProperty(style::Properties::kDontPolishFontProperty, true);
    ui->systemNameLabel->setForegroundRole(QPalette::Text);
    ui->systemNameEdit->setFont(font);
    ui->systemNameEdit->setProperty(style::Properties::kDontPolishFontProperty, true);

    auto buttonLayout = new QHBoxLayout(ui->buttonWidget);
    buttonLayout->setSpacing(0);

    auto paintButtonFunction = [this](QPainter* painter, const QStyleOption* option, const QWidget* widget) -> bool
    {
        auto button = static_cast<const QPushButton*>(widget);
        QRect rect = button->rect();

        if (option->state.testFlag(QStyle::State_Enabled))
        {
            if (button->isDown())
                painter->fillRect(button->rect(), option->palette.base());
            else if (option->state.testFlag(QStyle::State_MouseOver))
                painter->fillRect(button->rect(), option->palette.dark());
        }

        rect = rect.marginsRemoved(kPreferencesButtonMargins);
        QRect iconRect = QStyle::alignedRect(Qt::LeftToRight, Qt::AlignHCenter | Qt::AlignTop,
            QnSkin::maximumSize(button->icon()), rect);

        button->icon().paint(painter, iconRect);
        rect.setTop(iconRect.bottom() + 1);

        painter->drawText(rect, Qt::AlignCenter | Qt::TextWordWrap, button->text());

        return true;
    };

    for (auto& button : m_buttons)
    {
        button = new CustomPaintedButton(ui->buttonWidget);
        button->setFixedHeight(kPreferencesButtonSize);
        button->setMinimumWidth(kPreferencesButtonSize);
        button->setCustomPaintFunction(paintButtonFunction);
        buttonLayout->addWidget(button, 1);
    }

    m_buttons[kBusinessRulesButton]->setIcon(qnSkin->icon("system_settings/alarm-event_rules.png"));
    m_buttons[kEventLogButton     ]->setIcon(qnSkin->icon("system_settings/event_log.png"));
    m_buttons[kCameraListButton   ]->setIcon(qnSkin->icon("system_settings/cameras_list.png"));
    m_buttons[kAuditLogButton     ]->setIcon(qnSkin->icon("system_settings/audit_trail.png"));
    m_buttons[kHealthMonitorButton]->setIcon(qnSkin->icon("system_settings/health_monitoring.png"));
    m_buttons[kBookmarksButton    ]->setIcon(qnSkin->icon("system_settings/bookmarks.png"));

    retranslateUi();

    setHelpTopic(m_buttons[kBusinessRulesButton], Qn::EventsActions_Help);
    setHelpTopic(m_buttons[kEventLogButton     ], Qn::EventLog_Help);
    setHelpTopic(m_buttons[kCameraListButton   ], Qn::Administration_General_CamerasList_Help);
    setHelpTopic(m_buttons[kAuditLogButton     ], Qn::AuditTrail_Help);
    setHelpTopic(m_buttons[kHealthMonitorButton], Qn::Administration_General_HealthMonitoring_Help);
    setHelpTopic(m_buttons[kBookmarksButton    ], Qn::Bookmarks_Usage_Help);

    connect(m_buttons[kBusinessRulesButton], &QPushButton::clicked, this,
        [this] { menu()->trigger(QnActions::OpenBusinessRulesAction); });

    connect(m_buttons[kCameraListButton], &QPushButton::clicked, this,
        [this] { menu()->trigger(QnActions::CameraListAction); });

    connect(m_buttons[kAuditLogButton], &QPushButton::clicked, this,
        [this] { menu()->trigger(QnActions::OpenAuditLogAction); });

    connect(m_buttons[kEventLogButton], &QPushButton::clicked, this,
        [this] { menu()->trigger(QnActions::OpenBusinessLogAction); });

    connect(m_buttons[kHealthMonitorButton], &QPushButton::clicked, this,
        [this] { menu()->trigger(QnActions::OpenInNewLayoutAction, qnResPool->getResourcesWithFlag(Qn::server)); });

    connect(m_buttons[kBookmarksButton], &QPushButton::clicked, this,
        [this] { menu()->trigger(QnActions::OpenBookmarksSearchAction); });

    connect(ui->systemSettingsWidget, &QnAbstractPreferencesWidget::hasChangesChanged,
        this, &QnAbstractPreferencesWidget::hasChangesChanged);

#ifdef SYSTEM_NAME_EDITING_ENABLED
    auto editSystemName = [this]()
    {
        ui->systemNameEdit->setText(ui->systemNameLabel->text());
        ui->systemNameStackedWidget->setCurrentWidget(ui->editPage);
        ui->systemNameEdit->setFocus();
    };

    auto clickSignalizer = new QnSingleEventSignalizer(this);
    clickSignalizer->setEventType(QEvent::MouseButtonPress);
    ui->systemNameLabel->installEventFilter(clickSignalizer);
    connect(clickSignalizer, &QnSingleEventSignalizer::activated, this,
        [editSystemName](QObject* sender, QEvent* event)
        {
            Q_UNUSED(sender);
            if (static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton)
                editSystemName();
        });

    connect(ui->systemNameEditButton, &QToolButton::clicked, this, editSystemName);
    ui->systemNameEditButton->setVisible(true);
    ui->systemNameEditButton->setIcon(qnSkin->icon("system_settings/edit.png"));
    ui->systemNameEditButton->setFixedSize(QnSkin::maximumSize(ui->systemNameEditButton->icon()));
    ui->systemNameLabel->setCursor(Qt::IBeamCursor);

#else // SYSTEM_NAME_EDITING_ENABLED
    ui->systemNameEditButton->setVisible(false);
#endif
}

void QnGeneralSystemAdministrationWidget::loadDataToUi()
{
    ui->systemSettingsWidget->loadDataToUi();
    ui->backupGroupBox->setVisible(isDatabaseBackupAvailable());

    ui->systemNameLabel->setText(qnCommon->localSystemName());
    ui->systemNameStackedWidget->setCurrentWidget(ui->labelPage);
}

void QnGeneralSystemAdministrationWidget::applyChanges()
{
    ui->systemSettingsWidget->applyChanges();

#ifdef SYSTEM_NAME_EDITING_ENABLED
    NX_ASSERT(false, "System name editing is not implemented!");
#endif
}

bool QnGeneralSystemAdministrationWidget::hasChanges() const
{
#ifdef SYSTEM_NAME_EDITING_ENABLED
    if (!ui->systemNameEdit->isHidden() && ui->systemNameEdit->text().trimmed() != qnCommon->localSystemName())
        return true;
#endif

    return ui->systemSettingsWidget->hasChanges();
}

void QnGeneralSystemAdministrationWidget::retranslateUi()
{
    m_buttons[kBusinessRulesButton]->setText(tr("Alarm/Event Rules"));
    m_buttons[kEventLogButton     ]->setText(tr("Event Log"));
    m_buttons[kAuditLogButton     ]->setText(tr("Audit Trail"));
    m_buttons[kHealthMonitorButton]->setText(tr("Health Monitoring"));
    m_buttons[kBookmarksButton    ]->setText(tr("Bookmarks"));
    m_buttons[kCameraListButton   ]->setText(
        QnDeviceDependentStrings::getDefaultNameFromSet(tr("Device List"), tr("Camera List")));

    auto shortcutString = [this](const QnActions::IDType actionId, const QString &baseString) -> QString
    {
        auto shortcut = action(actionId)->shortcut();
        if (shortcut.isEmpty())
            return baseString;
        return lit("<div style='white-space: pre'>%1 (<b>%2</b>)</div>")
            .arg(baseString)
            .arg(shortcut.toString(QKeySequence::NativeText));
    };

    m_buttons[kBusinessRulesButton]->setToolTip(shortcutString(QnActions::BusinessEventsAction,
        tr("Open Alarm/Event Rules Management")));

    m_buttons[kEventLogButton]->setToolTip(shortcutString(QnActions::OpenBusinessLogAction,
        tr("Open Event Log")));

    m_buttons[kAuditLogButton]->setToolTip(shortcutString(QnActions::OpenAuditLogAction,
        tr("Open Audit Trail Log")));

    m_buttons[kBookmarksButton]->setToolTip(shortcutString(QnActions::OpenBookmarksSearchAction,
        tr("Open Bookmarks List")));

    m_buttons[kHealthMonitorButton]->setToolTip(
        tr("Monitor All Servers on a Single Layout"));

    m_buttons[kCameraListButton]->setToolTip(shortcutString(QnActions::CameraListAction,
        QnDeviceDependentStrings::getDefaultNameFromSet(tr("Open Device List"), tr("Open Camera List"))));

    ui->systemSettingsWidget->retranslateUi();
}

bool QnGeneralSystemAdministrationWidget::isDatabaseBackupAvailable() const
{
    return QnRuntimeInfoManager::instance()->remoteInfo().data.box != lit("isd");
}

void QnGeneralSystemAdministrationWidget::setReadOnlyInternal(bool readOnly)
{
    ::setReadOnly(ui->systemSettingsWidget, readOnly);
    ::setReadOnly(ui->backupWidget, readOnly);
}
