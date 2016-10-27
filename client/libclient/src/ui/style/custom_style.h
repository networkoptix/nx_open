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

