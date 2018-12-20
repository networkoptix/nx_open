#pragma once

#include <functional>
#include "helper.h"

class QWidget;
class QPalette;
class QPushButton;
class QTabWidget;
class QStackedWidget;

void setWarningStyle(QWidget* widget, qreal disabledOpacity = 1.0);
void setWarningStyle(QPalette* palette, qreal disabledOpacity = 1.0);
void setCustomStyle(QPalette* palette, const QColor& color, qreal disabledOpacity = 1.0);

/** Enable/disable warning style with a single call. */
void setWarningStyleOn(QWidget* widget, bool on, qreal disabledOpacity = 1.0);

QString setWarningStyleHtml(const QString& source);

void resetStyle(QWidget* widget);

void resetButtonStyle(QAbstractButton* button);
void setAccentStyle(QAbstractButton* button);
void setWarningButtonStyle(QAbstractButton* button);

void setTabShape(QTabBar* tabBar, style::TabShape tabShape);

void setMonospaceFont(QWidget* widget);
QFont monospaceFont(const QFont& font = QFont());

/**
 * Improves size limits for stacked widgets and tab widgets:
 *  they will be automatically adjusted at appropriate moments.
 *
 * Size policy of invisible pages will be set to { Ignored, Ignored },
 *  size policy of current page will be set to visiblePagePolicy.
 * If resizeToVisible is true, size limits will be computed by current page only,
 *  otherwise size limits will be computed by all pages. WARNING! In many cases invisible widgets
 *  don't properly compute their size hints; set resizeToVisible to false only if you're ABSOLUTELY
 *  sure what you're doing.
 * Optional extraHandler will be called after every adjustment of the limits.
 *  it can be used for extra layout processing (e.g. dialog shrinking to content).
 */
void autoResizePagesToContents(QStackedWidget* pages,
    QSizePolicy visiblePagePolicy,
    bool resizeToVisible = true,
    std::function<void()> extraHandler = std::function<void()>());

void autoResizePagesToContents(QTabWidget* pages,
    QSizePolicy visiblePagePolicy,
    bool resizeToVisible = true,
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

