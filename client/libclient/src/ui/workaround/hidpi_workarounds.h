
#pragma once

class QMenu;
class QAction;
class QToolButton;
class QPoint;

class QnHiDpiWorkarounds
{
public:
    static QPoint scaledToGlobal(const QPoint& scaled);

    static QAction* showMenu(QMenu* menu, const QPoint& globalPoint);

    static void showMenuOnWidget(QWidget* widget, const QPoint& offset, QMenu* menu);

    static void setViewportWindow(QWindow* window);
};