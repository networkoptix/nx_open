
#pragma once

class QMenu;
class QAction;
class QPoint;
class QMovie;
class QLabel;

class QnHiDpiWorkarounds
{
public:
    static QAction* showMenu(QMenu* menu, const QPoint& globalPoint);

    static QPoint safeMapToGlobal(QWidget*widget, const QPoint& offset);

    static void init();

    /**
     * Workaround for https://bugreports.qt.io/browse/QTBUG-48157
     * TODO: #ynikitenkov Remove this workaround after we come to
     * Qt 5.6.3+ version
     */
    static void setMovieToLabel(QLabel* label, QMovie* movie);
};
