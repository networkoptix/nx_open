#pragma once

#include <QtCore/Qt>
#include <QtCore/QMargins>

class QWindow;

QWindow *getMainWindow();

void prepareWindow();
void hideSystemUi();
void showSystemUi();

int statusBarHeight();
bool isLeftSideNavigationBar();
int navigationBarHeight();
bool isPhone();

void setKeepScreenOn(bool keepScreenOn);

void setScreenOrientation(Qt::ScreenOrientation orientation);

QMargins getCustomMargins();

void makeShortVibration();
