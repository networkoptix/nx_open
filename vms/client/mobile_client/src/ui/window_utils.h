// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMargins>
#include <QtCore/Qt>

class QWindow;

QWindow *getMainWindow();

void prepareWindow();
void hideSystemUi();
void showSystemUi();

int statusBarHeight();
bool isPhone();

void setKeepScreenOn(bool keepScreenOn);

void setScreenOrientation(Qt::ScreenOrientation orientation);

QMargins getCustomMargins();

void makeShortVibration();

bool is24HoursTimeFormat();

QString webkitUrl();

/**
 * Workaround for the QTBUG-72472 - view is not changing size if there is Android WebView on the
 * scene. Also keyboard height is always 0 in this situation in Qt.
 * Check if the bug is fixed in Qt 6.x.
 */
int androidKeyboardHeight();

/** Explicitly requests audio recording permissions if they are not granted yet. */
void requestRecordAudioPermissionIfNeeded();

/** Set exlusion area for the back gesture. */
void setAndroidGestureExclusionArea(int startY, int height);
