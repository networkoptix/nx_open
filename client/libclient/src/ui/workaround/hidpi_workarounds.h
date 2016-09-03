
#pragma once

class QMenu;
class QAction;
class QToolButton;
class QPoint;
class QComboBox;

class QnHiDpiWorkarounds
{
public:
    static QPoint scaledToGlobal(const QPoint& scaled);

    static QAction* showMenu(QMenu* menu, const QPoint& globalPoint);

    static void showMenuOnWidget(QWidget* widget, const QPoint& offset, QMenu* menu);

    static QPoint safeMapToGlobal(QWidget*widget, const QPoint& offset);

    static void init();
};