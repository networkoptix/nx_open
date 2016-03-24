#include "main_window_title_controls_widget.h"

#include <ui/actions/actions.h>
#include <ui/common/geometry.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/style/noptix_style.h>
#include <ui/widgets/cloud_status_panel.h>

QnMainWindowTitleControlsWidget::QnMainWindowTitleControlsWidget(QWidget* parent, QnWorkbenchContext* context) :
    base_type(parent),
    QnWorkbenchContextAware(parent, context)
{
    setFocusPolicy(Qt::NoFocus);

    QnCloudStatusPanel *cloudPanel = new QnCloudStatusPanel(this);
    cloudPanel->setFocusPolicy(Qt::NoFocus);

    /* Layout for window buttons that can be removed from the title bar. */
    QHBoxLayout* layout = new QHBoxLayout();
    layout->setContentsMargins(4, 0, 2, 0);
    layout->setSpacing(4);
    layout->addWidget(cloudPanel);
    layout->addWidget(newActionButton(action(QnActions::OpenLoginDialogAction), false, 1.0, Qn::Login_Help));
    layout->addWidget(newActionButton(action(QnActions::WhatsThisAction), false, 1.0, Qn::MainWindow_ContextHelp_Help));
    layout->addWidget(newActionButton(action(QnActions::MinimizeAction)));
    layout->addWidget(newActionButton(action(QnActions::EffectiveMaximizeAction), false, 1.0, Qn::MainWindow_Fullscreen_Help));
    layout->addWidget(newActionButton(action(QnActions::ExitAction)));

    setLayout(layout);
}

QToolButton* QnMainWindowTitleControlsWidget::newActionButton(QAction *action, bool popup /*= false*/, qreal sizeMultiplier /*= 1.0*/, int helpTopicId /*= Qn::Empty_Help*/)
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
