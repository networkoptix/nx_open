// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMargins>
#include <QtCore/Qt>

class QColor;
class QWindow;

QWindow *getMainWindow();

void prepareWindow();
void hideSystemUi();
void showSystemUi();

bool isPhone();

void setKeepScreenOn(bool keepScreenOn);

void setScreenOrientation(Qt::ScreenOrientation orientation);

void makeShortVibration();

bool is24HoursTimeFormat();

int statusBarHeight();

/**
 * Workaround for the QTBUG-72472 - view is not changing size if there is Android WebView on the
 * scene. Also keyboard height is always 0 in this situation in Qt.
 * Check if the bug is fixed in Qt 6.x.
 */
int androidKeyboardHeight();

/** Explicitly requests audio recording permissions if they are not granted yet. */
void requestRecordAudioPermissionIfNeeded();

/** Set exclusion area for the back gesture. */
void setAndroidGestureExclusionArea(int startY, int height);

/**
 * Paints the background of the native web views. QtWebView creates them opaque and white, gives no
 * API to restyle them, and layers them above the Qt scene, where no QML item can cover them.
 *
 * Must be called from the GUI thread. Paints nothing while no native view is attached.
 */
void setWebViewBackgroundColor(const QColor& color);
