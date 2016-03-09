#pragma once

class QWidget;
class QPalette;
class QPushButton;

void setWarningStyle(QWidget *widget);
void setWarningStyle(QPalette *palette);
QString setWarningStyleHtml(const QString &source);

void setAccentStyle(QAbstractButton* button, bool accent = true);
