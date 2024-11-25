// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_status_panel.h"

#include <QtGui/QPainter>
#include <QtGui/QResizeEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenu>

#include <nx/branding.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/utils/geometry.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/window_context.h>
#include <ui/common/palette.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <utils/common/delayed.h>

// TODO: #dklychkov Uncomment when cloud login is implemented
//#define DIRECT_CLOUD_CONNECT

using namespace nx::vms::client::desktop;
using nx::vms::client::core::CloudStatusWatcher;

namespace {

constexpr int kFontPixelSize = 12;
constexpr int kMinimumPanelWidth = 36;
constexpr int kSideMargin = 5;
constexpr int kMenuIconWidth = 20;

const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kArrowTheme =
{{QnIcon::Normal, {.primary = "dark17"}}};
const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kCloudTheme =
{{QnIcon::Normal, {.primary = "light4"}}};
const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kCloudDisabledTheme =
{{QnIcon::Normal, {.primary = "light4", .alpha=0.7}}};
const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kCloudRedTheme =
{{QnIcon::Normal, {.primary = "red"}}};
NX_DECLARE_COLORIZED_ICON(kArrowCloseIcon, "20x20/Solid/arrow_close.svg", kArrowTheme);
NX_DECLARE_COLORIZED_ICON(kArrowOpenIcon, "20x20/Solid/arrow_open.svg", kArrowTheme);

NX_DECLARE_COLORIZED_ICON(kCloudIcon, "20x20/Outline/cloud.svg", kCloudTheme);
NX_DECLARE_COLORIZED_ICON(kCloudDisabledIcon, "20x20/Outline/cloud.svg", kCloudDisabledTheme);
NX_DECLARE_COLORIZED_ICON(kCloudOfflineIcon, "20x20/Outline/cloud_offline.svg", kCloudRedTheme);

} // namespace

class QnCloudStatusPanelPrivate: public QObject
{
    QnCloudStatusPanel* q_ptr;
    Q_DECLARE_PUBLIC(QnCloudStatusPanel)
public:
    QnCloudStatusPanelPrivate(QnCloudStatusPanel* parent);

    void updateUi();
#ifdef DIRECT_CLOUD_CONNECT
    void updateSystems();
#endif

public:
    QMenu* loggedInMenu;
    QMenu* offlineMenu;
#ifdef DIRECT_CLOUD_CONNECT
    QMenu* systemsMenu;
#endif
    QIcon loggedInIcon;     /*< User is logged in. */
    QIcon loggedOutIcon;    /*< User is logged out. */
    QIcon offlineIcon;      /*< User is logged in, but cloud is unreachable. */

    bool isPressed{false};
};

QnCloudStatusPanel::QnCloudStatusPanel(
    nx::vms::client::desktop::WindowContext* windowContext,
    QWidget* parent)
    :
    base_type(parent),
    WindowContextAware(windowContext),
    d_ptr(new QnCloudStatusPanelPrivate(this))
{
    Q_D(QnCloudStatusPanel);

    setProperty(nx::style::Properties::kDontPolishFontProperty, true);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
    setPaletteColor(this, QPalette::Window, nx::vms::client::core::colorTheme()->color("dark7"));
    setContentsMargins(kSideMargin, 0, kSideMargin, 0);
    setPopupMode(QToolButton::InstantPopup);

    QFont font = qApp->font();
    font.setPixelSize(kFontPixelSize);
    font.setWeight(QFont::Bold);
    setFont(font);

    setHelpTopic(this, HelpTopic::Id::MainWindow_TitleBar_Cloud);

    connect(this, &QnCloudStatusPanel::justPressed, qnCloudStatusWatcher,
        &CloudStatusWatcher::updateSystems);

    connect(
        this,
        &QnCloudStatusPanel::clicked,
        this,
        [this]
        {
            // This handler is used instead of setting a default action is used because the default
            // action changes button appearance from icon button to text button.
            if (qnCloudStatusWatcher->status() == CloudStatusWatcher::LoggedOut)
                this->windowContext()->menu()->trigger(menu::LoginToCloud);
        });

    d->updateUi();
}

QnCloudStatusPanel::~QnCloudStatusPanel()
{
}

QSize QnCloudStatusPanel::minimumSizeHint() const
{
    auto base = base_type::minimumSizeHint();
    base = nx::vms::client::core::Geometry::dilated(base, contentsMargins());
    if (base.width() < kMinimumPanelWidth)
        base.setWidth(kMinimumPanelWidth);

    return base;
}

QSize QnCloudStatusPanel::sizeHint() const
{
    auto base = base_type::sizeHint();
    if (base.width() < kMinimumPanelWidth)
        base.setWidth(kMinimumPanelWidth);

    return base;
}

void QnCloudStatusPanel::resizeEvent(QResizeEvent* event)
{
    base_type::resizeEvent(event);

    d_ptr->loggedInMenu->setFixedWidth(event->size().width());
    d_ptr->offlineMenu->setFixedWidth(event->size().width());
}

void QnCloudStatusPanel::paintEvent(QPaintEvent* event)
{
    base_type::paintEvent(event);

    if (!static_cast<base_type*>(this)->menu())
        return;

    QPainter painter{this};

    QIcon icon = qnSkin->icon(isDown() ? kArrowCloseIcon : kArrowOpenIcon);
    icon.paint(
        &painter,
        QRect{width() - kMenuIconWidth - kSideMargin, 0, kMenuIconWidth, height()});
}

void QnCloudStatusPanel::mousePressEvent(QMouseEvent* e)
{
    if (d_ptr->isPressed)
        return;

    d_ptr->isPressed = true;

    // While the menu is opened the event loop is running in this function.
    base_type::mousePressEvent(e);

    // This workaround is required to support the behaviour when button click closes opened menu
    // instead opening it again.
    executeLater([this]{ d_ptr->isPressed = false; }, this);
}

QnCloudStatusPanelPrivate::QnCloudStatusPanelPrivate(QnCloudStatusPanel* parent):
    QObject(parent),
    q_ptr(parent),
    loggedInMenu(new QMenu(parent->mainWindowWidget())),
    offlineMenu(new QMenu(parent->mainWindowWidget())),
#ifdef DIRECT_CLOUD_CONNECT
    systemsMenu(nullptr),
#endif
    loggedInIcon(qnSkin->icon(kCloudIcon)),
    loggedOutIcon(qnSkin->icon(kCloudDisabledIcon)),
    offlineIcon(qnSkin->icon(kCloudOfflineIcon))
{
    Q_Q(QnCloudStatusPanel);
    loggedInMenu->setWindowFlags(loggedInMenu->windowFlags());
    loggedInMenu->addAction(q->action(menu::OpenCloudMainUrl));
    loggedInMenu->addAction(q->action(menu::OpenCloudManagementUrl));
    loggedInMenu->addAction(q->action(menu::LogoutFromCloud));

    auto offlineAction = new QAction(this);
    offlineAction->setText(QnCloudStatusPanel::tr("Cannot connect to %1",
        "%1 is the cloud name (like Nx Cloud)").arg(nx::branding::cloudName()));
    offlineAction->setEnabled(false);

    offlineMenu->setWindowFlags(offlineMenu->windowFlags());
    offlineMenu->addAction(offlineAction);
    offlineMenu->addSeparator();
    offlineMenu->addAction(q->action(menu::LogoutFromCloud));

    connect(qnCloudStatusWatcher, &CloudStatusWatcher::statusChanged, this,
        &QnCloudStatusPanelPrivate::updateUi);
    connect(qnCloudStatusWatcher, &CloudStatusWatcher::cloudLoginChanged, this,
        &QnCloudStatusPanelPrivate::updateUi);

#ifdef DIRECT_CLOUD_CONNECT
    systemsMenu = loggedInMenu->addMenu(QnCloudStatusPanel::tr("Connect to Server"));
    connect(qnCloudStatusWatcher, &CloudStatusWatcher::cloudSystemsChanged, this, &QnCloudStatusPanelPrivate::updateSystems);
    updateSystems();
#endif
}

void QnCloudStatusPanelPrivate::updateUi()
{
    Q_Q(QnCloudStatusPanel);

    QString effectiveUserName = qnCloudStatusWatcher->cloudLogin();
    if (effectiveUserName.isEmpty())
        effectiveUserName = QnCloudStatusPanel::tr("Logging in...");

    const auto cloudStatus = qnCloudStatusWatcher->status();
    switch (cloudStatus)
    {
        case CloudStatusWatcher::LoggedOut:
            q->setText(QString());
            q->setIcon(loggedOutIcon);
            q->setToolButtonStyle(Qt::ToolButtonIconOnly);
            q->setMenu(nullptr);
            q->setToolTip(QnCloudStatusPanel::tr(
                "Log in to %1", "%1 is the cloud name (like Nx Cloud)")
                    .arg(nx::branding::cloudName()));
            break;
        case CloudStatusWatcher::Online:
        case CloudStatusWatcher::UpdatingCredentials:
            q->setText(effectiveUserName);
            q->setIcon(loggedInIcon);
            q->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            q->setMenu(loggedInMenu);
            q->setToolTip(QnCloudStatusPanel::tr(
                "Logged in as %1", "%1 is the cloud login name (like user@domain.com")
                    .arg(effectiveUserName));
            break;
        case CloudStatusWatcher::Offline:
            q->setText(effectiveUserName);
            q->setIcon(offlineIcon);
            q->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            q->setMenu(offlineMenu);
            break;
        default:
            NX_ASSERT(false, "Should never get here.");
            break;
    }
    q->adjustIconSize();

    q->setContentsMargins(
        kSideMargin,
        0,
        kSideMargin + (cloudStatus == CloudStatusWatcher::LoggedOut ? 0 : kMenuIconWidth),
        0);
}

#ifdef DIRECT_CLOUD_CONNECT
void QnCloudStatusPanelPrivate::updateSystems()
{
    const QnCloudSystemList& systems = qnCloudStatusWatcher->cloudSystems();

    systemsMenu->clear();
    for (const QnCloudSystem& system : systems)
    {
        QAction* action = systemsMenu->addAction(system.name);
        QUrl url;
        // TODO: #dklychkov Prepare URL
        action->setData(url);
    }

    systemsMenu->menuAction()->setVisible(!systemsMenu->actions().isEmpty());
}
#endif
