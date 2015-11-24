#pragma once

#include <QtCore/QtGlobal>

void prepareWindow();
void hideSystemUi();
void showSystemUi();

int statusBarHeight();
int navigationBarHeight();
bool isPhone();

void setKeepScreenOn(bool keepScreenOn);
