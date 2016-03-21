#ifndef WARNING_STYLE_H
#define WARNING_STYLE_H

class QWidget;
class QPalette;

void setWarningStyle(QWidget *widget);
void setWarningStyle(QPalette *palette);
QString setWarningStyleHtml(const QString &source);

#endif // WARNING_STYLE_H
