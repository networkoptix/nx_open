#include "main_window_title_bar_widget.h"

#include <ui/actions/actions.h>
#include <ui/common/geometry.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/style/noptix_style.h>
#include <ui/widgets/cloud_status_panel.h>
#include <ui/widgets/layout_tab_bar.h>
#include <ui/workaround/qtbug_workaround.h>
#include <ui/actions/action_manager.h>
#include <ui/workbench/workbench_layout.h>

class QnMainWindowTitleBarWidgetPrivate : public QObject
{
    QnMainWindowTitleBarWidget* q_ptr;
    Q_DECLARE_PUBLIC(QnMainWindowTitleBarWidget)

public:
    QnMainWindowTitleBarWidgetPrivate(QnMainWindowTitleBarWidget* parent);

    void setSkipDoubleClick();

public:
    QToolButton* mainMenuButton;
    QnLayoutTabBar* tabBar;
    bool skipDoubleClickFlag;
};

QnMainWindowTitleBarWidgetPrivate::QnMainWindowTitleBarWidgetPrivate(
        QnMainWindowTitleBarWidget* parent)
    : QObject(parent)
    , q_ptr(parent)
    , mainMenuButton(nullptr)
    , tabBar(nullptr)
    , skipDoubleClickFlag(false)
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

    d->mainMenuButton = newActionButton(
            action(QnActions::MainMenuAction), true, 1.5,
            Qn::MainWindow_TitleBar_MainMenu_Help);

    d->tabBar = new QnLayoutTabBar(this);
    connect(d->tabBar, &QnLayoutTabBar::tabCloseRequested,  d, &QnMainWindowTitleBarWidgetPrivate::setSkipDoubleClick);
    connect(d->tabBar, &QnLayoutTabBar::currentChanged,     d, &QnMainWindowTitleBarWidgetPrivate::setSkipDoubleClick);
    connect(action(QnActions::MainMenuAction), &QAction::triggered, d, &QnMainWindowTitleBarWidgetPrivate::setSkipDoubleClick);

    connect(d->tabBar, &QnLayoutTabBar::closeRequested,
            this, [this](QnWorkbenchLayout* layout)
    {
        menu()->trigger(QnActions::CloseLayoutAction,
                        QnWorkbenchLayoutList() << layout);
    });

    QnCloudStatusPanel* cloudPanel = new QnCloudStatusPanel(this);
    cloudPanel->setFocusPolicy(Qt::NoFocus);

    /* Layout for window buttons that can be removed from the title bar. */
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(d->mainMenuButton);
    layout->addWidget(d->tabBar);
    layout->addWidget(newActionButton(action(QnActions::OpenNewTabAction), false, 1.0, Qn::MainWindow_TitleBar_NewLayout_Help));
    layout->addWidget(newActionButton(action(QnActions::OpenCurrentUserLayoutMenu), true));
    layout->addStretch(1);
    layout->addSpacing(80);
    layout->addWidget(cloudPanel);
    layout->addWidget(newActionButton(action(QnActions::OpenLoginDialogAction), false, 1.0, Qn::Login_Help));
    layout->addWidget(newActionButton(action(QnActions::WhatsThisAction), false, 1.0, Qn::MainWindow_ContextHelp_Help));
    layout->addWidget(newActionButton(action(QnActions::MinimizeAction)));
    layout->addWidget(newActionButton(action(QnActions::EffectiveMaximizeAction), false, 1.0, Qn::MainWindow_Fullscreen_Help));
    layout->addWidget(newActionButton(action(QnActions::ExitAction)));
}

QnMainWindowTitleBarWidget::~QnMainWindowTitleBarWidget()
{
}

QnLayoutTabBar* QnMainWindowTitleBarWidget::tabBar() const
{
    Q_D(const QnMainWindowTitleBarWidget);
    return d->tabBar;
}

QToolButton* QnMainWindowTitleBarWidget::newActionButton(QAction *action, bool popup /*= false*/, qreal sizeMultiplier /*= 1.0*/, int helpTopicId /*= Qn::Empty_Help*/)
{
    QToolButton *button = new QToolButton();
    button->setDefaultAction(action);

    qreal aspectRatio = QnGeometry::aspectRatio(action->icon().actualSize(QSize(1024, 1024)));
    int iconHeight = QApplication::style()->pixelMetric(QStyle::PM_ToolBarIconSize, 0, button) * sizeMultiplier;
    int iconWidth = iconHeight * aspectRatio;
    button->setFixedSize(iconWidth, iconHeight);
    button->setFocusPolicy(Qt::NoFocus);

    button->setProperty(Qn::ToolButtonCheckedRotationSpeed, action->property(Qn::ToolButtonCheckedRotationSpeed));

    if (popup) {
        /* We want the button to activate the corresponding action so that menu is updated.
        * However, menu buttons do not activate their corresponding actions as they do not receive release events.
        * We work this around by making some hacky connections. */
        button->setPopupMode(QToolButton::InstantPopup);

        QObject::disconnect(button, SIGNAL(pressed()), button, SLOT(_q_buttonPressed()));
        QObject::connect(button, SIGNAL(pressed()), button->defaultAction(), SLOT(trigger()));
        QObject::connect(button, SIGNAL(pressed()), button, SLOT(_q_buttonPressed()));
    }

    if (helpTopicId != Qn::Empty_Help)
        setHelpTopic(button, helpTopicId);

    return button;
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
