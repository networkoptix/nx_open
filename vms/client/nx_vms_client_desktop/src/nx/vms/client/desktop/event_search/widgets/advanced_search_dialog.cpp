// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "advanced_search_dialog.h"

#include <memory>

#include <QtGui/QGuiApplication>
#include <QtGui/QScreen>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtWidgets/QWidget>

#include <nx/utils/log/format.h>
#include <nx/utils/singleton.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/common/system_settings.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/event_processors.h>

#include "private/advanced_search_dialog_p.h"

template<>
nx::vms::client::desktop::AdvancedSearchDialog::StateDelegate*
    Singleton<nx::vms::client::desktop::AdvancedSearchDialog::StateDelegate>::s_instance = nullptr;

namespace nx::vms::client::desktop {

namespace {

static const QString kAdvancedSearchDialogKey = "AdvancedSearchDialog";
static const QString kWindowSizeKey = "windowSize";
static const QString kWindowPositionKey = "windowPosition";
static const QString kWindowMaximizedKey = "isMaximized";
static const QString kScreenNumberKey = "screenNumber";

} // namespace

// ------------------------------------------------------------------------------------------------
// AdvancedSearchDialog

AdvancedSearchDialog::AdvancedSearchDialog(QWidget* parent) :
    QmlDialogWrapper(
        QUrl("Nx/LeftPanel/private/analytics/AnalyticsSearchDialog.qml"),
        {},
        parent),
    QnSessionAwareDelegate(parent)
{
    rootObjectHolder()->object()->setProperty(
        "windowContext", QVariant::fromValue(windowContext()));

    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
        this, &QObject::deleteLater);

    // This callback is going to be called on QEvent::Hide of underlying QQuickWindow.
    // The call happens from its destructor, meaning that std::unique_ptr<QQuickWindow>
    // in QmlDialogWrapper::Private is empty. So in order to access the QQuickWindow object
    // the QPointer (for safety) is captured by the callback.
    const auto saveState =
        [this, quickWindow = QPointer<QQuickWindow>(window())]()
        {
            const auto state = StateDelegate::instance();
            if (!state || !quickWindow)
                return;

            const auto windowStates = quickWindow->windowStates();
            state->isMaximized = windowStates.testFlag(Qt::WindowMaximized);

            state->screen = quickWindow->screen() != defaultScreen()
                ? qApp->screens().indexOf(quickWindow->screen())
                : -1;

            if (state->isMaximized || windowStates.testFlag(Qt::WindowMinimized))
                return; //< Don't save size & position in minimized or maximized states.

            state->windowSize = quickWindow->size();
            state->windowPosition = quickWindow->position();
        };

    const auto restoreState =
        [this]()
        {
            const auto state = StateDelegate::instance();
            if (!state)
                return;

            const auto screen = state->screen >= 0 && state->screen < qApp->screens().size()
                ? qApp->screens()[state->screen]
                : defaultScreen();

            window()->setScreen(screen);
            setMaximized(state->isMaximized);

            if (!state->windowSize.isEmpty())
                window()->resize(state->windowSize);

            // Change default position only if saved position is valid (visible on a screen).
            if (state->windowPosition)
            {
                const QRect desiredRect(*state->windowPosition, window()->size());
                const auto screens = screen->virtualSiblings();

                for (auto sibling: screens)
                {
                    constexpr int kMinimumExposure = 64;
                    const QRect intersection = desiredRect.intersected(sibling->geometry());

                    const bool isOnScreen = intersection.width() >= kMinimumExposure
                        && intersection.height() >= kMinimumExposure;

                    if (isOnScreen)
                    {
                        window()->setPosition(*state->windowPosition);
                        break;
                    }
                }
            }
        };

    installEventHandler(window(), QEvent::Hide, this, saveState);
    installEventHandler(window(), QEvent::Show, this, restoreState);

    connect(StateDelegate::instance(), &StateDelegate::aboutToBeSaved, this, saveState);
    connect(StateDelegate::instance(), &StateDelegate::loaded, this, restoreState);
    restoreState();
}

void AdvancedSearchDialog::registerStateDelegate()
{
    appContext()->clientStateHandler()->registerDelegate(
        kAdvancedSearchDialogKey, std::make_unique<StateDelegate>());
}

void AdvancedSearchDialog::unregisterStateDelegate()
{
    appContext()->clientStateHandler()->unregisterDelegate(kAdvancedSearchDialogKey);
}

QScreen* AdvancedSearchDialog::defaultScreen() const
{
    return qApp->primaryScreen();
}

bool AdvancedSearchDialog::tryClose(bool /*force*/)
{
    reject();
    return true;
}

// ------------------------------------------------------------------------------------------------
// AdvancedSearchDialog::StateDelegate

bool AdvancedSearchDialog::StateDelegate::loadState(
    const DelegateState& state, SubstateFlags flags, const StartupParameters& /*params*/)
{
    if (!flags.testFlag(ClientStateDelegate::Substate::systemSpecificParameters))
        return false;

    isMaximized = state.value(kWindowMaximizedKey).toBool();
    screen = state.value(kScreenNumberKey).toInt(-1);

    windowSize = QSize();
    const auto sizeStrs = state.value(kWindowSizeKey).toString().split("x");
    if (sizeStrs.size() > 1)
        windowSize = QSize(sizeStrs[0].toInt(), sizeStrs[1].toInt());

    windowPosition = std::nullopt;
    const auto positionStrs = state.value(kWindowPositionKey).toString().split(";");
    if (positionStrs.size() > 1)
        windowPosition = QPoint(positionStrs[0].toInt(), positionStrs[1].toInt());

    emit loaded();
    return true;
}

void AdvancedSearchDialog::StateDelegate::saveState(DelegateState* state, SubstateFlags flags)
{
    if (!flags.testFlag(ClientStateDelegate::Substate::systemSpecificParameters))
        return;

    emit aboutToBeSaved();

    (*state)[kWindowMaximizedKey] = isMaximized;
    (*state)[kScreenNumberKey] = screen;

    (*state)[kWindowSizeKey] =
        nx::format("%1x%2", windowSize.width(), windowSize.height()).toQString();

    if (windowPosition)
    {
        (*state)[kWindowPositionKey] =
            nx::format("%1; %2", windowPosition->x(), windowPosition->y()).toQString();
    }
}

} // namespace nx::vms::client::desktop
