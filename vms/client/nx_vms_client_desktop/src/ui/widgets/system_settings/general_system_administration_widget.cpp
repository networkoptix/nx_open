// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "general_system_administration_widget.h"
#include "ui_general_system_administration_widget.h"

#include <QtGui/QAction>
#include <QtGui/QResizeEvent>
#include <QtWidgets/QPushButton>

#include <api/runtime_info_manager.h>
#include <common/common_module.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/string.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/utils/custom_painted.h>
#include <nx/vms/client/desktop/common/widgets/hint_button.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/system_settings.h>
#include <ui/common/read_only.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/event_processors.h>

#include "system_settings_widget.h"

using namespace nx::vms::client::desktop;
using namespace nx::vms::common;

namespace {

static const int kMaxSystemNameLabelWidth = 1000;
static const int kMaxSystemNameLength = 64;

static const int kPreferencesButtonSize = 104;
static const QMargins kPreferencesButtonMargins(8, 4, 8, 4);

static const QColor kLight4Color = "#E1E7EA";
static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kNormalIconSubstitutions = {
    {QIcon::Normal, {{kLight4Color, "light4"}}},
};

} // namespace

QnGeneralSystemAdministrationWidget::QnGeneralSystemAdministrationWidget(
    nx::vms::api::SaveableSystemSettings* editableSystemSettings,
    QWidget* parent)
    :
    AbstractSystemSettingsWidget(editableSystemSettings, parent),
    ui(new Ui::GeneralSystemAdministrationWidget),
    m_systemSettingsWidget(new QnSystemSettingsWidget(editableSystemSettings, this))
{
    ui->setupUi(this);
    ui->systemSettingsGroupBox->layout()->addWidget(m_systemSettingsWidget);

    ui->systemNameLabel->setMaximumWidth(kMaxSystemNameLabelWidth);
    ui->systemNameLabel->setValidator(
        [](QString& text) -> bool
        {
            text = text.trimmed().left(kMaxSystemNameLength);
            return !text.isEmpty();
        });

    connect(ui->systemNameLabel, &EditableLabel::textChanging,
        this, &QnGeneralSystemAdministrationWidget::hasChangesChanged);
    connect(ui->systemNameLabel, &EditableLabel::editingFinished,
        this, &QnGeneralSystemAdministrationWidget::hasChangesChanged);
    connect(systemSettings(), &SystemSettings::systemNameChanged,
        this, &QnGeneralSystemAdministrationWidget::loadDataToUi);

    auto buttonLayout = new QHBoxLayout(ui->buttonWidget);
    buttonLayout->setSpacing(0);

    auto paintButtonFunction =
        [this](QPainter* painter, const QStyleOption* option, const QWidget* widget) -> bool
        {
            auto button = static_cast<const QPushButton*>(widget);
            QRect rect = button->rect();

            const bool enabled = option->state.testFlag(QStyle::State_Enabled);
            const bool hasFocus = option->state.testFlag(QStyle::State_HasFocus);

            if (enabled)
            {
                if (button->isDown())
                    painter->fillRect(button->rect(), option->palette.base());
                else if (option->state.testFlag(QStyle::State_MouseOver))
                    painter->fillRect(button->rect(), option->palette.dark());
            }

            rect = rect.marginsRemoved(kPreferencesButtonMargins);
            QRect iconRect = QStyle::alignedRect(Qt::LeftToRight, Qt::AlignHCenter | Qt::AlignTop,
                nx::vms::client::core::Skin::maximumSize(button->icon()), rect);

            button->icon().paint(painter, iconRect);
            rect.setTop(iconRect.bottom() + 1);

            painter->drawText(rect, Qt::AlignCenter | Qt::TextWordWrap, button->text());

            if (hasFocus && enabled)
                style()->drawPrimitive(QStyle::PE_FrameFocusRect, option, painter, widget);

            return true;
        };

    QWidget* prevTabItem = ui->buttonWidget;
    for (auto& button: m_buttons)
    {
        button = new CustomPaintedButton(ui->buttonWidget);
        button->setFixedHeight(kPreferencesButtonSize);
        button->setMinimumWidth(kPreferencesButtonSize);
        button->setCustomPaintFunction(paintButtonFunction);
        buttonLayout->addWidget(button, 1);
        setTabOrder(prevTabItem, button);
        prevTabItem = button;
    }

    ui->buttonWidget->setFocusPolicy(Qt::NoFocus);

    m_buttons[kBusinessRulesButton]->setIcon(
        qnSkin->icon("system_settings/event_rules_56.svg", kNormalIconSubstitutions));
    m_buttons[kEventLogButton     ]->setIcon(
        qnSkin->icon("system_settings/event_log_56.svg", kNormalIconSubstitutions));
    m_buttons[kCameraListButton   ]->setIcon(
        qnSkin->icon("system_settings/cameras_list_56.svg", kNormalIconSubstitutions));
    m_buttons[kAuditLogButton     ]->setIcon(
        qnSkin->icon("system_settings/audit_trail_56.svg", kNormalIconSubstitutions));
    m_buttons[kBookmarksButton    ]->setIcon(
        qnSkin->icon("system_settings/bookmarks_56.svg", kNormalIconSubstitutions));
    m_buttons[kIntegrationsButton ]->setIcon(
        qnSkin->icon("system_settings/integrations_56.svg", kNormalIconSubstitutions));

    m_buttons[kIntegrationsButton]->setVisible(ini().integrationsManagement);

    retranslateUi();

    setHelpTopic(m_buttons[kBusinessRulesButton], HelpTopic::Id::EventsActions);
    setHelpTopic(m_buttons[kEventLogButton], HelpTopic::Id::EventLog);
    setHelpTopic(m_buttons[kCameraListButton],
        HelpTopic::Id::Administration_General_CamerasList);
    setHelpTopic(m_buttons[kAuditLogButton], HelpTopic::Id::AuditTrail);
    setHelpTopic(m_buttons[kBookmarksButton], HelpTopic::Id::Bookmarks_Search);
    setHelpTopic(m_buttons[kIntegrationsButton], HelpTopic::Id::PluginsAndAnalytics);

    connect(m_buttons[kBusinessRulesButton], &QPushButton::clicked, this,
        [this] { menu()->trigger(menu::OpenBusinessRulesAction); });

    connect(m_buttons[kCameraListButton], &QPushButton::clicked, this,
        [this] { menu()->trigger(menu::CameraListAction); });

    connect(m_buttons[kAuditLogButton], &QPushButton::clicked, this,
        [this] { menu()->trigger(menu::OpenAuditLogAction); });

    connect(m_buttons[kEventLogButton], &QPushButton::clicked, this,
        [this] { menu()->trigger(menu::OpenBusinessLogAction); });

    connect(m_buttons[kBookmarksButton], &QPushButton::clicked, this,
        [this] { menu()->trigger(menu::OpenBookmarksSearchAction); });

    connect(m_buttons[kIntegrationsButton], &QPushButton::clicked, this,
        [this] { menu()->trigger(menu::OpenIntegrationsAction); });

    connect(m_systemSettingsWidget, &QnAbstractPreferencesWidget::hasChangesChanged,
        this, &QnAbstractPreferencesWidget::hasChangesChanged);
}

void QnGeneralSystemAdministrationWidget::loadDataToUi()
{
    ui->systemNameLabel->setText(systemSettings()->systemName());
    m_systemSettingsWidget->loadDataToUi();
}

void QnGeneralSystemAdministrationWidget::applyChanges()
{
    m_systemSettingsWidget->applyChanges();
    ui->systemNameLabel->setEditing(false);
    editableSystemSettings->systemName = ui->systemNameLabel->text().trimmed();
}

bool QnGeneralSystemAdministrationWidget::hasChanges() const
{
    return (ui->systemNameLabel->text().trimmed() != systemSettings()->systemName())
        || m_systemSettingsWidget->hasChanges();
}

void QnGeneralSystemAdministrationWidget::retranslateUi()
{
    m_buttons[kBusinessRulesButton]->setText(tr("Event Rules"));
    m_buttons[kEventLogButton     ]->setText(tr("Event Log"));
    m_buttons[kAuditLogButton     ]->setText(tr("Audit Trail"));
    m_buttons[kBookmarksButton    ]->setText(tr("Bookmarks"));
    m_buttons[kIntegrationsButton ]->setText(tr("Integrations"));
    m_buttons[kCameraListButton   ]->setText(
        QnDeviceDependentStrings::getDefaultNameFromSet(resourcePool(), tr("Device List"), tr("Camera List")));

    auto shortcutString = [this](const menu::IDType actionId, const QString &baseString) -> QString
    {
        auto shortcut = action(actionId)->shortcut();
        if (shortcut.isEmpty())
            return baseString;
        return lit("<div style='white-space: pre'>%1 (<b>%2</b>)</div>")
            .arg(baseString)
            .arg(shortcut.toString(QKeySequence::NativeText));
    };

    m_buttons[kBusinessRulesButton]->setToolTip(shortcutString(menu::BusinessEventsAction,
        tr("Open Event Rules Management")));

    m_buttons[kEventLogButton]->setToolTip(shortcutString(menu::OpenBusinessLogAction,
        tr("Open Event Log")));

    m_buttons[kAuditLogButton]->setToolTip(shortcutString(menu::OpenAuditLogAction,
        tr("Open Audit Trail Log")));

    m_buttons[kBookmarksButton]->setToolTip(shortcutString(menu::OpenBookmarksSearchAction,
        tr("Open Bookmarks List")));

    m_buttons[kIntegrationsButton]->setToolTip(shortcutString(menu::OpenIntegrationsAction,
        tr("Open Integrations")));

    m_buttons[kCameraListButton]->setToolTip(shortcutString(menu::CameraListAction,
        QnDeviceDependentStrings::getDefaultNameFromSet(
            resourcePool(),
            tr("Open Device List"),
            tr("Open Camera List"))));

    m_systemSettingsWidget->retranslateUi();
}

void QnGeneralSystemAdministrationWidget::setReadOnlyInternal(bool readOnly)
{
    ::setReadOnly(m_systemSettingsWidget, readOnly);
}
