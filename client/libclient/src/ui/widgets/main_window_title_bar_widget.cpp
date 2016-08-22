#include "main_window_title_bar_widget.h"

#include <core/resource/media_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/layout_resource.h>

#include <ui/actions/actions.h>
#include <ui/common/geometry.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/widgets/cloud_status_panel.h>
#include <ui/widgets/layout_tab_bar.h>
#include <ui/widgets/common/tool_button.h>
#include <ui/workaround/qtbug_workaround.h>
#include <ui/actions/action_manager.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_resource.h>

namespace {
const int kTitleBarHeight = 24;
const int kVLineWidth = 1;
const QSize kControlButtonSize(40 - kVLineWidth, kTitleBarHeight);
const auto kTabBarButtonSize = QSize(kTitleBarHeight, kTitleBarHeight);

QToolButton* newActionButton(
    QAction* action,
    int helpTopicId = Qn::Empty_Help,
    bool popup = false,
    const QSize& fixedSize = QSize())
{
    QnToolButton* button = new QnToolButton();

    button->setDefaultAction(action);
    button->setFocusPolicy(Qt::NoFocus);
    button->adjustIconSize();
    button->setFixedSize(fixedSize.isEmpty() ? button->iconSize() : fixedSize);

    if (popup)
    {
        button->setPopupMode(QToolButton::InstantPopup);
        QObject::connect(button, &QnToolButton::justPressed, [action]()
        {
            action->trigger();
        });
    }

    if (helpTopicId != Qn::Empty_Help)
        setHelpTopic(button, helpTopicId);

    return button;
}

QToolButton* newActionButton(
    QAction* action,
    bool popup,
    const QSize& fixedSize = QSize())
{
    return newActionButton(action, Qn::Empty_Help, popup, fixedSize);
}

QFrame* newVLine()
{
    QFrame* line = new QFrame();
    line->setFrameShape(QFrame::VLine);
    line->setFrameShadow(QFrame::Plain);
    line->setFixedWidth(1);
    return line;
}
}

class QnMainWindowTitleBarWidgetPrivate: public QObject
{
    QnMainWindowTitleBarWidget* q_ptr;
    Q_DECLARE_PUBLIC(QnMainWindowTitleBarWidget)

public:
    QnMainWindowTitleBarWidgetPrivate(QnMainWindowTitleBarWidget* parent);

    void setSkipDoubleClick();

public:
    QToolButton* mainMenuButton;
    QnLayoutTabBar* tabBar;
    QToolButton* newTabButton;
    QToolButton* currentLayoutsButton;
    QnCloudStatusPanel* cloudPanel;
    QnResourceList dropResources;
    bool skipDoubleClickFlag;
};

QnMainWindowTitleBarWidgetPrivate::QnMainWindowTitleBarWidgetPrivate(
    QnMainWindowTitleBarWidget* parent)
    :
    QObject(parent),
    q_ptr(parent),
    mainMenuButton(nullptr),
    tabBar(nullptr),
    newTabButton(nullptr),
    currentLayoutsButton(nullptr),
    cloudPanel(nullptr),
    skipDoubleClickFlag(false)
{
}

void QnMainWindowTitleBarWidgetPrivate::setSkipDoubleClick()
{
    skipDoubleClickFlag = true;
}

QnMainWindowTitleBarWidget::QnMainWindowTitleBarWidget(
    QWidget* parent,
    QnWorkbenchContext* context)
    : base_type(parent)
    , QnWorkbenchContextAware(parent, context)
    , d_ptr(new QnMainWindowTitleBarWidgetPrivate(this))

{
    Q_D(QnMainWindowTitleBarWidget);

    setFocusPolicy(Qt::NoFocus);
    setAutoFillBackground(true);
    setFixedHeight(kTitleBarHeight);
    setAcceptDrops(true);

    d->mainMenuButton = newActionButton(
        action(QnActions::MainMenuAction),
        Qn::MainWindow_TitleBar_MainMenu_Help,
        true);

    d->tabBar = new QnLayoutTabBar(this);
    d->tabBar->setFixedHeight(kTitleBarHeight);
    connect(d->tabBar, &QnLayoutTabBar::tabCloseRequested, d, &QnMainWindowTitleBarWidgetPrivate::setSkipDoubleClick);
    connect(d->tabBar, &QnLayoutTabBar::closeRequested,
        this, [this](QnWorkbenchLayout* layout)
    {
        menu()->trigger(QnActions::CloseLayoutAction,
            QnWorkbenchLayoutList() << layout);
    });

    d->cloudPanel = new QnCloudStatusPanel(this);
    d->cloudPanel->setFocusPolicy(Qt::NoFocus);
    d->cloudPanel->setFixedHeight(kTitleBarHeight);

    /* Layout for window buttons that can be removed from the title bar. */
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(d->mainMenuButton);
    layout->addWidget(newVLine());
    layout->addWidget(d->tabBar);
    layout->addWidget(newVLine());

    d->newTabButton = newActionButton(action(QnActions::OpenNewTabAction),
        Qn::MainWindow_TitleBar_NewLayout_Help, false, kTabBarButtonSize);
    d->currentLayoutsButton = newActionButton(action(QnActions::OpenCurrentUserLayoutMenu),
        true, kTabBarButtonSize);
    layout->addWidget(d->newTabButton);
    layout->addWidget(d->currentLayoutsButton);
    layout->addStretch(1);
    layout->addSpacing(80);
    layout->addWidget(newVLine());
    layout->addWidget(d->cloudPanel);
    layout->addWidget(newVLine());
    layout->addWidget(newActionButton(
        action(QnActions::OpenLoginDialogAction),
        Qn::Login_Help, false, kControlButtonSize));
    layout->addWidget(newVLine());
    layout->addWidget(newActionButton(
        action(QnActions::WhatsThisAction),
        Qn::MainWindow_ContextHelp_Help, false, kControlButtonSize));
    layout->addWidget(newVLine());
    layout->addWidget(newActionButton(
        action(QnActions::MinimizeAction),
        false, kControlButtonSize));
    layout->addWidget(newVLine());
    layout->addWidget(newActionButton(
        action(QnActions::EffectiveMaximizeAction),
        Qn::MainWindow_Fullscreen_Help, false, kControlButtonSize));
    layout->addWidget(newVLine());
    layout->addWidget(newActionButton(
        action(QnActions::ExitAction),
        false, kControlButtonSize));
}

QnMainWindowTitleBarWidget::~QnMainWindowTitleBarWidget()
{
}

QnLayoutTabBar* QnMainWindowTitleBarWidget::tabBar() const
{
    Q_D(const QnMainWindowTitleBarWidget);
    return d->tabBar;
}

void QnMainWindowTitleBarWidget::setTabBarStuffVisible(bool visible)
{
    Q_D(const QnMainWindowTitleBarWidget);
    d->tabBar->setVisible(visible);
    d->newTabButton->setVisible(visible);
    d->currentLayoutsButton->setVisible(visible);
    action(QnActions::OpenNewTabAction)->setEnabled(visible);
}

bool QnMainWindowTitleBarWidget::event(QEvent* event)
{
    Q_D(QnMainWindowTitleBarWidget);

    bool result = base_type::event(event);

    if (event->type() == QnEvent::WinSystemMenu)
    {
        if (d->mainMenuButton->isVisible())
            d->mainMenuButton->click();

        result = true;
    }

    return result;
}

void QnMainWindowTitleBarWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    Q_D(QnMainWindowTitleBarWidget);

    base_type::mouseDoubleClickEvent(event);

#ifndef Q_OS_MACX
    if (d->skipDoubleClickFlag)
    {
        d->skipDoubleClickFlag = false;
        return;
    }

    action(QnActions::EffectiveMaximizeAction)->toggle();
    event->accept();
#endif
}

void QnMainWindowTitleBarWidget::dragEnterEvent(QDragEnterEvent* event)
{
    Q_D(QnMainWindowTitleBarWidget);
    QnResourceList resources = QnWorkbenchResource::deserializeResources(event->mimeData());

    QnResourceList media;
    QnResourceList layouts;
    QnResourceList servers;

    for (auto res : resources)
    {
        if (dynamic_cast<QnMediaResource*>(res.data()))
            media << res;

        if (res.dynamicCast<QnLayoutResource>())
            layouts << res;

        if (res.dynamicCast<QnMediaServerResource>())
            servers << res;
    }

    d->dropResources = media;
    d->dropResources << layouts;
    d->dropResources << servers;

    if (d->dropResources.empty())
        return;

    event->acceptProposedAction();
}

void QnMainWindowTitleBarWidget::dragMoveEvent(QDragMoveEvent* event)
{
    Q_D(QnMainWindowTitleBarWidget);
    if (d->dropResources.empty())
        return;

    if (!qBetween(d->tabBar->pos().x(), event->pos().x(), d->cloudPanel->pos().x()))
        return;

    event->acceptProposedAction();
}

void QnMainWindowTitleBarWidget::dragLeaveEvent(QDragLeaveEvent* event)
{
    Q_UNUSED(event);
    Q_D(QnMainWindowTitleBarWidget);
    d->dropResources = QnResourceList();
}

void QnMainWindowTitleBarWidget::dropEvent(QDropEvent* event)
{
    Q_D(QnMainWindowTitleBarWidget);
    menu()->trigger(QnActions::DropResourcesIntoNewLayoutAction, d->dropResources);
    event->acceptProposedAction();
}
