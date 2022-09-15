// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "general_system_administration_widget.h"
#include "ui_general_system_administration_widget.h"

#include <QtGui/QResizeEvent>
#include <QtWidgets/QAction>
#include <QtWidgets/QPushButton>

#include <api/runtime_info_manager.h>
#include <common/common_module.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/string.h>
#include <nx/vms/client/desktop/common/utils/custom_painted.h>
#include <nx/vms/client/desktop/common/widgets/hint_button.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/system_settings.h>
#include <ui/common/read_only.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/event_processors.h>

using namespace nx::vms::client::desktop;
using namespace nx::vms::common;

namespace {

static const int kMaxSystemNameLabelWidth = 1000;
static const int kMaxSystemNameLength = 64;

static const int kPreferencesButtonSize = 104;
static const QMargins kPreferencesButtonMargins(8, 4, 8, 4);

} // namespace

QnGeneralSystemAdministrationWidget::QnGeneralSystemAdministrationWidget(QWidget *parent /* = nullptr*/) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::GeneralSystemAdministrationWidget)
{
    ui->setupUi(this);

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
    connect(globalSettings(), &SystemSettings::systemNameChanged,
        this, &QnGeneralSystemAdministrationWidget::loadSystemName);

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
                Skin::maximumSize(button->icon()), rect);

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

    m_buttons[kBusinessRulesButton]->setIcon(qnSkin->icon("system_settings/event_rules.png"));
    m_buttons[kEventLogButton     ]->setIcon(qnSkin->icon("system_settings/event_log.png"));
    m_buttons[kCameraListButton   ]->setIcon(qnSkin->icon("system_settings/cameras_list.png"));
    m_buttons[kAuditLogButton     ]->setIcon(qnSkin->icon("system_settings/audit_trail.png"));
    m_buttons[kBookmarksButton    ]->setIcon(qnSkin->icon("system_settings/bookmarks.png"));

    retranslateUi();

    setHelpTopic(m_buttons[kBusinessRulesButton], Qn::EventsActions_Help);
    setHelpTopic(m_buttons[kEventLogButton     ], Qn::EventLog_Help);
    setHelpTopic(m_buttons[kCameraListButton   ], Qn::Administration_General_CamerasList_Help);
    setHelpTopic(m_buttons[kAuditLogButton     ], Qn::AuditTrail_Help);
    setHelpTopic(m_buttons[kBookmarksButton], Qn::Bookmarks_Search_Help);

    connect(m_buttons[kBusinessRulesButton], &QPushButton::clicked, this,
        [this] { menu()->trigger(ui::action::OpenBusinessRulesAction); });

    connect(m_buttons[kCameraListButton], &QPushButton::clicked, this,
        [this] { menu()->trigger(ui::action::CameraListAction); });

    connect(m_buttons[kAuditLogButton], &QPushButton::clicked, this,
        [this] { menu()->trigger(ui::action::OpenAuditLogAction); });

    connect(m_buttons[kEventLogButton], &QPushButton::clicked, this,
        [this] { menu()->trigger(ui::action::OpenBusinessLogAction); });

    connect(m_buttons[kBookmarksButton], &QPushButton::clicked, this,
        [this] { menu()->trigger(ui::action::OpenBookmarksSearchAction); });

    connect(ui->systemSettingsWidget, &QnAbstractPreferencesWidget::hasChangesChanged,
        this, &QnAbstractPreferencesWidget::hasChangesChanged);
}

void QnGeneralSystemAdministrationWidget::loadDataToUi()
{
    loadSystemName();
    ui->systemSettingsWidget->loadDataToUi();
}

void QnGeneralSystemAdministrationWidget::applyChanges()
{
    ui->systemSettingsWidget->applyChanges();
    ui->systemNameLabel->setEditing(false);

    globalSettings()->setSystemName(ui->systemNameLabel->text().trimmed());
    globalSettings()->synchronizeNow();
}

bool QnGeneralSystemAdministrationWidget::hasChanges() const
{
    return (ui->systemNameLabel->text().trimmed() != globalSettings()->systemName())
        || ui->systemSettingsWidget->hasChanges();
}

void QnGeneralSystemAdministrationWidget::retranslateUi()
{
    m_buttons[kBusinessRulesButton]->setText(tr("Event Rules"));
    m_buttons[kEventLogButton     ]->setText(tr("Event Log"));
    m_buttons[kAuditLogButton     ]->setText(tr("Audit Trail"));
    m_buttons[kBookmarksButton    ]->setText(tr("Bookmarks"));
    m_buttons[kCameraListButton   ]->setText(
        QnDeviceDependentStrings::getDefaultNameFromSet(resourcePool(), tr("Device List"), tr("Camera List")));

    auto shortcutString = [this](const ui::action::IDType actionId, const QString &baseString) -> QString
    {
        auto shortcut = action(actionId)->shortcut();
        if (shortcut.isEmpty())
            return baseString;
        return lit("<div style='white-space: pre'>%1 (<b>%2</b>)</div>")
            .arg(baseString)
            .arg(shortcut.toString(QKeySequence::NativeText));
    };

    m_buttons[kBusinessRulesButton]->setToolTip(shortcutString(ui::action::BusinessEventsAction,
        tr("Open Event Rules Management")));

    m_buttons[kEventLogButton]->setToolTip(shortcutString(ui::action::OpenBusinessLogAction,
        tr("Open Event Log")));

    m_buttons[kAuditLogButton]->setToolTip(shortcutString(ui::action::OpenAuditLogAction,
        tr("Open Audit Trail Log")));

    m_buttons[kBookmarksButton]->setToolTip(shortcutString(ui::action::OpenBookmarksSearchAction,
        tr("Open Bookmarks List")));

    m_buttons[kCameraListButton]->setToolTip(shortcutString(ui::action::CameraListAction,
        QnDeviceDependentStrings::getDefaultNameFromSet(
            resourcePool(),
            tr("Open Device List"),
            tr("Open Camera List"))));

    ui->systemSettingsWidget->retranslateUi();
}

void QnGeneralSystemAdministrationWidget::loadSystemName()
{
    ui->systemNameLabel->setText(globalSettings()->systemName());
}

void QnGeneralSystemAdministrationWidget::setReadOnlyInternal(bool readOnly)
{
    ::setReadOnly(ui->systemSettingsWidget, readOnly);
}
