
#pragma once

class QMenu;
class QAction;
class QToolButton;
class QPoint;
class QComboBox;

class QnHiDpiWorkarounds
{
public:
    static QPoint safeMapToGlobal(QWidget*widget, const QPoint& offset);

    static void init();
};