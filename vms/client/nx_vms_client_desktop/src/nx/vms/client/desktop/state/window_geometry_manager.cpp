// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "window_geometry_manager.h"

#include <QtCore/QRect>
#include <QtCore/QScopedPointer>
#include <QtGui/QScreen>
#include <QtWidgets/QApplication>
#include <QtWidgets/QStyle>

#include <nx/build_info.h>
#include <nx/fusion/serialization/json_functions.h>
#include <nx/reflect/json.h>
#include <nx/vms/client/desktop/ini.h>
#include <utils/common/delayed.h>

namespace nx::vms::client::desktop {

namespace {

const QString kWindowGeometryKey = "geometry";

// If client window is not fullscreen, child window inherits its geometry with these offsets.
const int kWindowShiftX = 20;
const int kWindowShiftY = 20;

// Maximum size of new window (in proportion to the size a screen at which this window is created).
const double kWindowScreenRatioX = 0.75;
const double kWindowScreenRatioY = 0.75;

// Minimum size of new window.
const int kMinWindowWidth = 800;
const int kMinWindowHeight = 600;

DelegateState serializeState(const WindowGeometryState& geometry)
{
    DelegateState state;
    QJson::deserialize(
        QString::fromStdString(nx::reflect::json::serialize(geometry)),
        &state[kWindowGeometryKey]);
    return state;
}

} // namespace

const QString WindowGeometryManager::kWindowGeometryData = "windowGeometry";

struct WindowGeometryManager::Private
{
    Private(WindowControlInterfacePtr control):
        control(std::move(control))
    {
    }

    WindowControlInterfacePtr control;
};

WindowGeometryManager::WindowGeometryManager(
    WindowControlInterfacePtr control, QObject* parent):
    base_type(parent),
    d(new Private(std::move(control)))
{
}

WindowGeometryManager::~WindowGeometryManager()
{
}

bool WindowGeometryManager::loadState(
    const DelegateState& state,
    SubstateFlags flags,
    const StartupParameters& params)
{
    bool success = false;

    if (flags.testFlag(ClientStateDelegate::Substate::systemIndependentParameters))
    {
        WindowGeometryState geometry;
        success = nx::reflect::json::deserialize(
            QJson::serialized(state.value(kWindowGeometryKey)).toStdString(), &geometry);

        if (params.noFullScreen)
            geometry.isFullscreen = geometry.isMaximized = false;

        if (!params.windowGeometry.isNull())
            geometry.geometry = params.windowGeometry;

        if (params.screen >= 0)
        {
            // If screen is passed using command line, client is supposed to run in fullscreen.
            if (!params.noFullScreen)
                geometry.isFullscreen = geometry.isMaximized = true;

            const auto surface = d->control->suitableSurface();
            if (params.screen < surface.size())
            {
                const auto screenRect = surface.at(params.screen);
                if (!screenRect.contains(geometry.geometry))
                {
                    // Put the window into the screen rect.
                    geometry.geometry.setWidth(qMax(
                        kMinWindowWidth,
                        (int)(kWindowScreenRatioX * surface[params.screen].width())));
                    geometry.geometry.setHeight(qMax(
                        kMinWindowHeight,
                        (int)(kWindowScreenRatioY * surface[params.screen].height())));
                    geometry.geometry.moveCenter(screenRect.center());
                }
            }
        }
        else
        {
            fixWindowGeometryIfNeeded(&geometry);
        }

        setWindowGeometry(geometry);
        // Developer builds have an issue when console window is placed onto screen with another
        // dpi value. Restored state is incorrect in this case and requires additional fix.
        if (ini().doubleGeometrySet)
            executeDelayedParented([this, geometry]{ setWindowGeometry(geometry); }, this);

        reportStatistics("window_fullscreen", geometry.isFullscreen);
        reportStatistics("window_position", geometry.geometry.topLeft());
        if (!geometry.isFullscreen)
            reportStatistics("window_dimensions", geometry.geometry.size());
    }

    return success;
}

void WindowGeometryManager::saveState(DelegateState* state, SubstateFlags flags)
{
    if (flags.testFlag(ClientStateDelegate::Substate::systemIndependentParameters))
    {
        *state = serializeState(windowGeometry());
    }
}

void WindowGeometryManager::createInheritedState(
    DelegateState* state,
    SubstateFlags flags,
    const QStringList& /*resources*/)
{
    if (flags.testFlag(ClientStateDelegate::Substate::systemIndependentParameters))
    {
        const auto surface = d->control->suitableSurface();
        const auto screenFor =
            [surface](const QPoint& p)
            {
                for (int i = 0; i < surface.count(); ++i)
                {
                    const auto& screenRect = surface[i];
                    if (screenRect.contains(p))
                        return i;
                }
                return -1;
            };

        WindowGeometryState inherited = windowGeometry();
        auto& geometry = inherited.geometry;

        if (inherited.isFullscreen || inherited.isMaximized)
        {
            // New fullscreen window. If possible, it should be placed on an empty screen.
            const auto source = d->control->windowScreen();
            const auto target = qMax(
                d->control->nextFreeScreen(),
                0); //< QGuiApplication::primaryScreen() always returns first element of screens().

            // It's not possible to create fullscreen window at some specific screen directly,
            // instead we should place a normal window at this screen and than make it fullscreen.

            // Update normal window size, so it wouldn't be larger than 3/4 of the target screen.
            geometry.setWidth(qMax(
                kMinWindowWidth,
                qMin(geometry.width(), (int)(kWindowScreenRatioX * surface[target].width()))));
            geometry.setHeight(qMax(
                kMinWindowHeight,
                qMin(geometry.height(), (int)(kWindowScreenRatioY * surface[target].height()))));

            // If new window could be placed using the same top&left margins on the target screen,
            // these margins will be used. Otherwise they are decreased to place the whole window
            // on the target screen.
            const auto top = qMin(
                geometry.top() - surface[source].top(),
                surface[target].bottom() - geometry.height());
            const auto left = qMin(
                geometry.left() - surface[source].left(),
                surface[target].right() - geometry.width());
            geometry.moveTopLeft(
                QPoint(surface[target].left() + left, surface[target].top() + top));
        }
        else
        {
            // Normal window geometry is inherited with some shift below and to the right.
            geometry.translate(kWindowShiftX, kWindowShiftY);

            // If it's not possible (i.e. the new window crosses screen border),
            // child window should be placed at the top left corner of the current screen.
            if (screenFor(geometry.topLeft()) != screenFor(geometry.bottomRight()))
            {
                // Note that we can't use screenFor() with some predefined point because user may
                // manually place existing window into such position that this point would not
                // belong to any screen.
                const int currentScreen = d->control->windowScreen();
                geometry.moveTo(surface[currentScreen].topLeft());
            }
        }

        *state = serializeState(inherited);
    }
}

WindowGeometryState WindowGeometryManager::calculateDefaultGeometry() const
{

    WindowGeometryState window;

    // Calculate default window size.
    const auto screenRect = (d->control->suitableSurface().size() > 0)
        ? d->control->suitableSurface().first()
        : QRect(0, 0, kMinWindowWidth, kMinWindowHeight); //< Safe option. Should be unreachable.
    const auto windowWidth = qMax(
        (int)(kWindowScreenRatioX * screenRect.width()),
        kMinWindowWidth);
    const auto windowHeight = qMax(
        (int)(kWindowScreenRatioY * screenRect.height()),
        kMinWindowHeight);

    // Safe option: place the window in top-left corner of the screen.
    window.geometry = QRect(screenRect.left(), screenRect.top(), windowWidth, windowHeight);
    // Adjust window position if the screen is large enought.
    if (screenRect.width() >= windowWidth && screenRect.height() >= windowHeight)
        window.geometry.moveCenter(screenRect.center());

    // Show normalized window on Mac, show fullscreen window on Windows/Linux.
    window.isFullscreen = window.isMaximized = !nx::build_info::isMacOsX();

    return window;
}

DelegateState WindowGeometryManager::defaultState() const
{
    return serializeState(calculateDefaultGeometry());
}

WindowGeometryState WindowGeometryManager::windowGeometry() const
{
    WindowGeometryState result;
    result.geometry = d->control->windowRect();
    result.isFullscreen = d->control->windowState() & Qt::WindowFullScreen;
    result.isMaximized = d->control->windowState() & Qt::WindowMaximized;
    return result;
}

void WindowGeometryManager::setWindowGeometry(const WindowGeometryState& value)
{
    NX_DEBUG(this, "Geometry: %1", nx::reflect::json::serialize(value));
    const auto currentState = d->control->windowState();
    if ((currentState.testFlag(Qt::WindowFullScreen) && value.isFullscreen)
        || (currentState.testFlag(Qt::WindowMaximized) && value.isMaximized))
    {
        return;
    }
    if (currentState.testAnyFlags(Qt::WindowFullScreen | Qt::WindowMaximized)
        && !value.isFullscreen
        && !value.isMaximized)
    {
        d->control->setWindowState(Qt::WindowNoState);
    }

    if (!value.geometry.isEmpty())
        d->control->setWindowRect(value.geometry);

    Qt::WindowStates state = Qt::WindowNoState;
    if (value.isFullscreen)
        state |= Qt::WindowFullScreen;
    if (value.isMaximized)
        state |= Qt::WindowMaximized;
    d->control->setWindowState(state);
}

bool WindowGeometryManager::isValid(const WindowGeometryState& value) const
{
    const auto surface = d->control->suitableSurface();
    return std::any_of(
        surface.begin(),
        surface.end(),
        [value](const QRect& s)
        {
            return s.intersects(value.geometry);
            // TODO: #spanasenko Improve this check. Check DPI and other stuff.
            // At least, we want to be sure that user can move and resize the created window.
        });
}

void WindowGeometryManager::fixWindowGeometryIfNeeded(WindowGeometryState* state) const
{
    bool onScreen = false;
    const QSize headerSize(
        state->geometry.width(),
        QApplication::style()->pixelMetric(QStyle::PM_TitleBarHeight));
    const QRect headerGeometry(state->geometry.topLeft(), headerSize);

    QRect boundingGeometry;
    for (const QScreen* screen: QGuiApplication::screens())
    {
        boundingGeometry |= screen->geometry();

        if (screen->geometry().intersects(headerGeometry))
            onScreen = true;
    }

    if (onScreen)
    {
        state->geometry &= boundingGeometry;
        state->geometry.setWidth(std::max(state->geometry.width(), kMinWindowWidth));
        state->geometry.setHeight(std::max(state->geometry.height(), kMinWindowHeight));
    }
    else
    {
        *state = calculateDefaultGeometry();
    }
}

} // namespace nx::vms::client::desktop
