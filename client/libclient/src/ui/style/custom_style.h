#pragma once

#include "helper.h"

class QWidget;
class QPalette;
class QPushButton;

void setWarningStyle(QWidget *widget);
void setWarningStyle(QPalette *palette);
QString setWarningStyleHtml(const QString &source);

void setAccentStyle(QAbstractButton* button, bool accent = true);

void setTabShape(QTabBar* tabBar, style::TabShape tabShape);
