#pragma once

#include <memory>

class QWidget;
class QMenu;
class QAction;
class QPoint;
class QMovie;
class QLabel;
class QEvent;
class QGraphicsSceneEvent;

class QnHiDpiWorkarounds
{
public:
    static QAction* showMenu(QMenu* menu, const QPoint& globalPoint);

    static QPoint safeMapToGlobal(const QWidget* widget, const QPoint& offset);

    /**
     * Qt 5.11.1 version
     * Hi dpi screen pos in events is computed differently for top level and child widgets.
     * It leads to a bunch of problems. We attempt to fix it by recomputing from local pos.
     */
    static QEvent* fixupEvent(QWidget* widget, QEvent* source, std::unique_ptr<QEvent>& target);
    static bool fixupGraphicsSceneEvent(QEvent* event); //< Modifies event without making a copy.

    static void init();

    /**
     * Workaround for https://bugreports.qt.io/browse/QTBUG-48157
     * TODO: #ynikitenkov Remove this workaround after we come to
     * Qt 5.6.3+ version
     */
    static void setMovieToLabel(QLabel* label, QMovie* movie);
};
