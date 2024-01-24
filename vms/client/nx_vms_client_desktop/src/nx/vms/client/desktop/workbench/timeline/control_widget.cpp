// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "control_widget.h"

#include <QtGui/QAction>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>

#include <camera/resource_display.h>
#include <client/client_runtime_settings.h>
#include <core/resource/resource.h>
#include <nx/streaming/abstract_archive_stream_reader.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/icon.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>
#include <nx/vms/client/desktop/statistics/context_statistics_module.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/workbench/extensions/workbench_stream_synchronizer.h>
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


// TODO: @pprivalov Remove this old fashioned color substitutions when figma plugin is ready.
static const QColor kBasePrimaryColor = "#ffffff";
static const QColor kThumbnailsColor = "#e1e7ea";
static const QColor kSoundColor = "#A5B7C0";
static const QColor kUnmuteColor = "#698796";
static const QColor kUnmuteCrossColor = "#C22626";
static const QColor kBackgroundColor = "#212A2F";
static const QColor kCheckedColor = "#171C1F";

// { Normal, Disabled, Active, Selected, Pressed, Error }
const core::SvgIconColorer::IconSubstitutions kNavigationIconSubstitutions =
{
    { QnIcon::Normal, {
        { kBasePrimaryColor, "light1"},
        { kThumbnailsColor, "light4"},
        { kSoundColor, "light10"},
        { kUnmuteColor, "light16"},
        { kBackgroundColor, "dark7"},
    }},
    { QnIcon::Disabled, {
        { kBasePrimaryColor, "dark11"},
        { kThumbnailsColor, "light11"},
        { kSoundColor, "light10"},
        { kUnmuteColor, "light16"},
        { kBackgroundColor, "dark6" },
    }},
    { QnIcon::Active, {  //< Hovered
        { kBasePrimaryColor, "light1"},
        { kThumbnailsColor, "light4"},
        { kSoundColor, "light10"},
        { kUnmuteColor, "light16"},
        { kBackgroundColor, "dark8" },
    }},
    { QnIcon::Pressed, {
        { kBasePrimaryColor, "light1"},
        { kThumbnailsColor, "light4"},
        { kSoundColor, "light10"},
        { kUnmuteColor, "light16"},
        { kBackgroundColor, "dark5"},
    }},
};

// { Normal, Disabled, Active, Selected, Pressed, Error }
const core::SvgIconColorer::IconSubstitutions kNavigationIconCheckedSubstitutions =
{
    { QnIcon::Normal, {
        { kBasePrimaryColor, "light1"},
        { kThumbnailsColor, "light4"},
        { kSoundColor, "light10"},
        { kUnmuteColor, "light16"},
        { kUnmuteCrossColor, "red_core"},
        { kBackgroundColor, "dark7"},
        { kCheckedColor, "green_l3"},
    }},
    { QnIcon::Disabled, {
        { kBasePrimaryColor, "dark11"},
        { kThumbnailsColor, "light4"},
        { kSoundColor, "light10"},
        { kUnmuteColor, "light16"},
        { kUnmuteCrossColor, "red_core"},
        { kBackgroundColor, "dark6" },
        { kCheckedColor, "green_l3"},
    }},
    { QnIcon::Active, {  //< Hovered
        { kBasePrimaryColor, "light1"},
        { kThumbnailsColor, "light4"},
        { kSoundColor, "light10"},
        { kUnmuteColor, "light16"},
        { kUnmuteCrossColor, "red_core"},
        { kBackgroundColor, "dark8" },
        { kCheckedColor, "green_l3"},
    }},
    { QnIcon::Pressed, {
        { kBasePrimaryColor, "light1"},
        { kThumbnailsColor, "light4"},
        { kSoundColor, "light10"},
        { kUnmuteColor, "light16"},
        { kUnmuteCrossColor, "red_core"},
        { kBackgroundColor, "dark5"},
        { kCheckedColor, "green_l3"},
    }},
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
        "slider/buttons/sound_24.svg", "slider/buttons/unmute_24.svg");

    initButton(m_liveButton, ui::action::JumpToLiveAction,
        "slider/buttons/live_52x24.svg",
        /*checkedIconPath*/ "",
        /*connectToAction*/ false);
    connect(m_liveButton, &QAbstractButton::clicked, this,
        [this]()
        {
            menu()->trigger(action::JumpToLiveAction, navigator()->currentWidget());
        });

    initButton(m_syncButton, ui::action::ToggleSyncAction,
        "slider/buttons/sync_52x24.svg");
    initButton(m_thumbnailsButton, ui::action::ToggleThumbnailsAction,
        "slider/buttons/thumbnails_52x24.svg");
    initButton(m_calendarButton, ui::action::ToggleCalendarAction,
        "slider/buttons/calendar_52x24.svg");

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

    setHelpTopic(this, HelpTopic::Id::MainWindow_Playback);
    setHelpTopic(m_volumeSlider, HelpTopic::Id::MainWindow_Slider_Volume);
    setHelpTopic(m_muteButton, HelpTopic::Id::MainWindow_Slider_Volume);
    setHelpTopic(m_liveButton, HelpTopic::Id::MainWindow_Navigation);
    setHelpTopic(m_syncButton, HelpTopic::Id::MainWindow_Sync);
    setHelpTopic(m_calendarButton, HelpTopic::Id::MainWindow_Calendar);
    setHelpTopic(m_thumbnailsButton, HelpTopic::Id::MainWindow_Thumbnails);

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
        ? qnSkin->icon(iconPath,
            kNavigationIconSubstitutions,
            checkedIconPath,
            kNavigationIconCheckedSubstitutions)
        : qnSkin->icon(iconPath,
            kNavigationIconSubstitutions,
            kNavigationIconCheckedSubstitutions));

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
    const auto currentResource = currentWidget ? currentWidget->resource() : QnResourcePtr();

    const bool bookmarksEnabled = currentResource
        && ResourceAccessManager::hasPermissions(currentResource, Qn::ViewBookmarksPermission)
        && currentResource->flags().testFlag(Qn::live)
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
