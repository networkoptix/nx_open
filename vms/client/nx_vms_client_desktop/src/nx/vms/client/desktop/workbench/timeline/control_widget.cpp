// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "control_widget.h"

#include <QtWidgets/QAction>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>

#include <camera/resource_display.h>
#include <client/client_runtime_settings.h>
#include <core/resource/resource.h>
#include <nx/streaming/abstract_archive_stream_reader.h>
#include <nx/vms/client/desktop/statistics/context_statistics_module.h>
#include <nx/vms/client/desktop/style/icon.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/common/color_theme.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/workbench/extensions/workbench_stream_synchronizer.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/event_processors.h>

#include "volume_slider.h"

namespace nx::vms::client::desktop::workbench::timeline {

namespace {

bool paintButtonFunction(QPainter* painter, const QStyleOption* /*option*/, const QWidget* widget)
{
    const QPushButton* thisButton = qobject_cast<const QPushButton*>(widget);

    QIcon::Mode mode = QnIcon::Normal;

    if (!thisButton->isEnabled())
        mode = QnIcon::Disabled;
    else if (thisButton->isDown())
        mode = QnIcon::Pressed;
    else if (thisButton->underMouse())
        mode = QnIcon::Active;

    thisButton->icon().paint(painter, thisButton->rect(), Qt::AlignCenter,
        mode, thisButton->isChecked() ? QIcon::On : QIcon::Off);

    return true;
};

} // namespace

using namespace ui;

ControlWidget::ControlWidget(QnWorkbenchContext* context, QWidget* parent):
    QWidget(parent),
    QnWorkbenchContextAware(context),
    m_volumeSlider(new VolumeSlider(context, this)),
    m_muteButton(new CustomPaintedButton(this)),
    m_liveButton(new CustomPaintedButton(this)),
    m_syncButton(new CustomPaintedButton(this)),
    m_thumbnailsButton(new CustomPaintedButton(this)),
    m_calendarButton(new CustomPaintedButton(this))
{
    installEventHandler({this}, {QEvent::Resize, QEvent::Move},
        this, &ControlWidget::geometryChanged);

    initButton(m_muteButton, ui::action::ToggleMuteAction,
        "slider/buttons/unmute.png", "slider/buttons/mute.png");

    initButton(m_liveButton, ui::action::JumpToLiveAction,
        "slider/buttons/live.png",
        /*checkedIconPath*/ "",
        /*connectToAction*/ false);
    connect(m_liveButton, &QAbstractButton::clicked, this,
        [this]()
        {
            menu()->trigger(action::JumpToLiveAction, navigator()->currentWidget());
        });

    initButton(m_syncButton, ui::action::ToggleSyncAction,
        "slider/buttons/sync.png");
    initButton(m_thumbnailsButton, ui::action::ToggleThumbnailsAction,
        "slider/buttons/thumbnails.png");
    initButton(m_calendarButton, ui::action::ToggleCalendarAction,
        "slider/buttons/calendar.png");

    statisticsModule()->controls()->registerSlider(
        "volume_slider",
        m_volumeSlider);

    auto mainLayout = new QVBoxLayout();
    mainLayout->setSpacing(2);

    auto volumeLayout = new QHBoxLayout();
    volumeLayout->setSpacing(3);
    volumeLayout->addWidget(m_muteButton);
    volumeLayout->addWidget(m_volumeSlider);
    mainLayout->addLayout(volumeLayout);

    auto buttonGridLayout = new QGridLayout();
    buttonGridLayout->setSpacing(2);
    buttonGridLayout->addWidget(m_liveButton, 0, 0);
    buttonGridLayout->addWidget(m_syncButton, 0, 1);
    buttonGridLayout->addWidget(m_thumbnailsButton, 1, 0);
    buttonGridLayout->addWidget(m_calendarButton, 1, 1);
    mainLayout->addLayout(buttonGridLayout);

    setLayout(mainLayout);

    /* Set up handlers. */
    auto streamSynchronizer = workbench()->windowContext()->streamSynchronizer();
    connect(streamSynchronizer, &QnWorkbenchStreamSynchronizer::runningChanged,
        this, &ControlWidget::updateSyncButtonState);
    connect(streamSynchronizer, &QnWorkbenchStreamSynchronizer::effectiveChanged,
        this, &ControlWidget::updateSyncButtonState);

    connect(m_volumeSlider, &VolumeSlider::valueChanged,
        this, &ControlWidget::updateMuteButtonChecked);

    connect(action(action::JumpToLiveAction), &QAction::triggered,
        this, &ControlWidget::at_jumpToliveAction_triggered);
    connect(action(action::ToggleSyncAction), &QAction::triggered,
        this, &ControlWidget::at_toggleSyncAction_triggered);

    connect(action(action::ToggleMuteAction), &QAction::toggled,
        m_volumeSlider, &VolumeSlider::setMute);
    connect(action(action::VolumeUpAction), &QAction::triggered,
        m_volumeSlider, &VolumeSlider::stepForward);
    connect(action(action::VolumeDownAction), &QAction::triggered,
        m_volumeSlider, &VolumeSlider::stepBackward);

    connect(navigator(), &QnWorkbenchNavigator::currentWidgetAboutToBeChanged, this, [this]()
    {
        const auto currentWidget = navigator()->currentWidget();
        if (currentWidget)
            disconnect(currentWidget, &QnResourceWidget::optionsChanged, this, nullptr);
    });

    connect(navigator(), &QnWorkbenchNavigator::currentWidgetChanged, this, [this]()
    {
        const auto currentWidget = navigator()->currentWidget();
        if (currentWidget)
            connect(currentWidget, &QnResourceWidget::optionsChanged, this, &ControlWidget::updateBookButtonEnabled);
    });

    connect(navigator(), &QnWorkbenchNavigator::currentWidgetChanged,
        this, &ControlWidget::updateSyncButtonState);
    connect(navigator(), &QnWorkbenchNavigator::syncIsForcedChanged,
        this, &ControlWidget::updateSyncButtonState);

    connect(navigator(), &QnWorkbenchNavigator::currentWidgetChanged,
        this, &ControlWidget::updateBookButtonEnabled);

    connect(
        navigator(),
        &QnWorkbenchNavigator::liveChanged,
        [this]()
        {
            m_liveButton->setChecked(navigator()->isLiveSupported() && navigator()->isLive());
        });

    connect(
        navigator(),
        &QnWorkbenchNavigator::liveSupportedChanged,
        [this]()
        {
            m_liveButton->setEnabled(navigator()->isLiveSupported());
        });

    connect(navigator(), &QnWorkbenchNavigator::playingSupportedChanged,
        this, &ControlWidget::updateVolumeButtonsEnabled);


    setHelpTopic(this,               Qn::MainWindow_Playback_Help);
    setHelpTopic(m_volumeSlider,     Qn::MainWindow_Slider_Volume_Help);
    setHelpTopic(m_muteButton,       Qn::MainWindow_Slider_Volume_Help);
    setHelpTopic(m_liveButton,       Qn::MainWindow_Navigation_Help);
    setHelpTopic(m_syncButton,       Qn::MainWindow_Sync_Help);
    setHelpTopic(m_calendarButton,   Qn::MainWindow_Calendar_Help);
    setHelpTopic(m_thumbnailsButton, Qn::MainWindow_Thumbnails_Help);

    /* Run handlers */
    updateMuteButtonChecked();
    updateSyncButtonState();
    updateLiveButtonState();
    updateVolumeButtonsEnabled();
}

void ControlWidget::setTooltipsVisible(bool enabled)
{
    m_volumeSlider->setTooltipsVisible(enabled);
}

void ControlWidget::initButton(
    CustomPaintedButton* button,
    ui::action::IDType actionType,
    const QString& iconPath,
    const QString& checkedIconPath,
    bool connectToAction)
{
    QAction* buttonAction = action(actionType);

    button->setCustomPaintFunction(paintButtonFunction);
    button->setIcon(!checkedIconPath.isEmpty()
        ? qnSkin->icon(iconPath, checkedIconPath)
        : qnSkin->icon(iconPath));

    const bool smallIcon = !checkedIconPath.isEmpty();
    button->setObjectName(buttonAction->text());
    button->setFixedSize(smallIcon ? QSize{24, 24} : QSize{52, 24} );
    button->setToolTip(buttonAction->toolTip());
    button->setCheckable(true);

    if (!connectToAction)
        return;

    connect(button, &QPushButton::clicked, buttonAction, &QAction::trigger);
    connect(
        buttonAction,
        &QAction::toggled,
        [button, buttonAction](bool isChecked)
        {
            button->setChecked(isChecked);
        });
    connect(
        buttonAction,
        &QAction::changed,
        [button, buttonAction]()
        {
            button->setToolTip(buttonAction->toolTip());
            button->setEnabled(buttonAction->isEnabled());
        });
}

// -------------------------------------------------------------------------- //
// Updaters
// -------------------------------------------------------------------------- //
void ControlWidget::updateBookButtonEnabled()
{
    const auto currentWidget = navigator()->currentWidget();

    const bool bookmarksEnabled =
        accessController()->hasGlobalPermission(GlobalPermission::viewBookmarks)
        && (currentWidget && currentWidget->resource()->flags().testFlag(Qn::live))
        && !qnRuntime->isAcsMode();

    const auto modeAction = action(action::BookmarksModeAction);
    modeAction->setEnabled(bookmarksEnabled);
}

void ControlWidget::updateVolumeButtonsEnabled()
{
    bool isTimelineVisible = navigator()->isPlayingSupported();
    m_muteButton->setEnabled(isTimelineVisible);
}

void ControlWidget::updateMuteButtonChecked()
{
    m_muteButton->setChecked(m_volumeSlider->isMute());
}

void ControlWidget::updateLiveButtonState()
{
    /* setEnabled must be called last to avoid update from button's action enabled state. */
    bool enabled = navigator()->isLiveSupported();
    m_liveButton->setChecked(enabled && navigator()->isLive());
    m_liveButton->setEnabled(enabled);
}

void ControlWidget::updateSyncButtonState()
{
    auto streamSynchronizer = context()->workbench()->windowContext()->streamSynchronizer();
    const auto syncForced = navigator()->syncIsForced();

    const auto syncAllowed = streamSynchronizer->isEffective()
        && navigator()->currentWidgetFlags().testFlag(QnWorkbenchNavigator::WidgetSupportsSync);

    // Call setEnabled last to avoid update from button's action enabled state.
    m_syncButton->setEnabled(syncAllowed && !syncForced);
    action(ui::action::ToggleSyncAction)->setChecked(syncAllowed && streamSynchronizer->isRunning());

    m_syncButton->setToolTip(syncForced
        ? tr("NVR cameras do not support not-synchronized playback")
        : action(action::ToggleSyncAction)->toolTip());
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void ControlWidget::at_jumpToliveAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    const auto widget = parameters.widget<QnMediaResourceWidget>();
    const bool synced = m_syncButton->isEnabled() && m_syncButton->isChecked();

    if (synced || widget == navigator()->currentWidget())
    {
        // Reset speed. It MUST be done before setLive(true) is called.
        navigator()->setSpeed(1.0);
        navigator()->setLive(true);
        action(action::PlayPauseAction)->setChecked(true);
    }
    else
    {
        const auto reader = widget && widget->display()
            ? widget->display()->archiveReader()
            : nullptr;

        if (reader)
        {
            reader->jumpTo(DATETIME_NOW, 0);
            reader->setSpeed(1.0);
            reader->resumeMedia();
        }
    }

    updateLiveButtonState();
}

void ControlWidget::at_toggleSyncAction_triggered()
{
    auto streamSynchronizer = workbench()->windowContext()->streamSynchronizer();

    if (m_syncButton->isChecked())
        streamSynchronizer->setState(navigator()->currentWidget());
    else
        streamSynchronizer->setState(nullptr);
}

} // nx::vms::client::desktop::workbench::timeline
