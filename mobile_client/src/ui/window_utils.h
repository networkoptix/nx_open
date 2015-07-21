#ifndef WINDOW_UTILS_H
#define WINDOW_UTILS_H

#include <QtCore/QtGlobal>

void prepareWindow();
void hideSystemUi();
void showSystemUi();

int statusBarHeight();
int navigationBarHeight();
bool isPhone();

#endif // WINDOW_UTILS_H
