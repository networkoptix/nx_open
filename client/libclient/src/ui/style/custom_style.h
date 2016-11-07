#pragma once

#include <functional>
#include "helper.h"

class QWidget;
class QPalette;
class QPushButton;

void setWarningStyle(QWidget *widget);
void setWarningStyle(QPalette *palette);
QString setWarningStyleHtml(const QString &source);

void setAccentStyle(QAbstractButton* button, bool accent = true);

void setTabShape(QTabBar* tabBar, style::TabShape tabShape);

void setMonospaceFont(QWidget* widget);
QFont monospaceFont(const QFont& font = QFont());

/*
* Improves size hint for stacked widgets and tab widgets.
* Sets { Ignored, Ignored } size policy for invisible pages initially and
*  if resizeToVisible is true sets it every time a page becomes invisible.
* Sets visiblePagePolicy for a page every time it becomes visible.
* Calls extraHandler, if set, initially and after every page change;
*  it can be used for extra layout processing (e.g. dialog shrinking to content).
*/
void autoResizePagesToContents(QStackedWidget* pages,
    QSizePolicy visiblePagePolicy,
    bool resizeToVisible,
    std::function<void()> extraHandler = std::function<void()>());

void autoResizePagesToContents(QTabWidget* pages,
    QSizePolicy visiblePagePolicy,
    bool resizeToVisible,
    std::function<void()> extraHandler = std::function<void()>());

/*
* Fade a widget in or out using QGraphicsOpacityEffect.
* Intended for use in dialogs.
*/
void fadeWidget(
    QWidget* widget,
    qreal initialOpacity, /* -1 for opacity the widget has at the moment of the call */
    qreal targetOpacity,
    int delayTimeMs = 0,
    qreal fadeSpeed /* opacity units per second */ = 1.0,
    std::function<void()> finishHandler = std::function<void()>(),
    int animationFps = 30);

