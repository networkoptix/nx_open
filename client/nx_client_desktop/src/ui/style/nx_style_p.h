#pragma once

#include <QtWidgets/QCommonStyle>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/private/qcommonstyle_p.h>

#include "nx_style.h"
#include "generic_palette.h"
#include "noptix_style_animator.h"
#include "helper.h"

class QInputDialog;
class QScrollBar;

class QnNxStylePrivate: public QCommonStylePrivate
{
    Q_DECLARE_PUBLIC(QnNxStyle)

public:
    explicit QnNxStylePrivate();
    virtual ~QnNxStylePrivate() override;

    QnPaletteColor findColor(const QColor& color) const;
    QnPaletteColor mainColor(QnNxStyle::Colors::Palette palette) const;

    QColor checkBoxColor(const QStyleOption* option, bool radio = false) const;

    void drawSwitch(
            QPainter* painter,
            const QStyleOption* option,
            const QWidget* widget = nullptr) const;

    void drawCheckBox(
            QPainter* painter,
            const QStyleOption* option,
            const QWidget* widget = nullptr) const;

    void drawRadioButton(
            QPainter* painter,
            const QStyleOption* option,
            const QWidget* widget = nullptr) const;

    void drawSortIndicator(
            QPainter* painter,
            const QStyleOption* option,
            const QWidget* widget = nullptr) const;

    void drawCross(
            QPainter* painter,
            const QRect& rect,
            const QColor& color) const;

    void drawTextButton(
            QPainter* painter,
            const QStyleOptionButton* option,
            QPalette::ColorRole foregroundRole,
            const QWidget* widget = nullptr) const;

    static void drawArrowIndicator(const QStyle* style,
        const QStyleOptionToolButton* toolbutton,
        const QRect& rect,
        QPainter* painter,
        const QWidget* widget = nullptr);

    void drawToolButton(QPainter* painter,
        const QStyleOptionToolButton* option,
        const QWidget* widget = nullptr) const;

    static bool isCheckableButton(const QStyleOption* option);

    /** TextButton is a PushButton which looks like a simple text (without bevel). */
    static bool isTextButton(const QStyleOption* option);

    /* Insert horizontal separator line into QInputDialog above its button box. */
    bool polishInputDialog(QInputDialog* inputDialog) const;

    /* Update QScrollArea hover if scrollBar is parented by one. */
    void updateScrollAreaHover(QScrollBar* scrollBar) const;

    /* Unlike QWidget::graphicsProxyWidget finds proxy recursively. */
    static QGraphicsProxyWidget* graphicsProxyWidget(const QWidget* widget);
    static const QWidget* graphicsProxiedWidget(const QWidget* widget);

public:
    QnGenericPalette palette;
    QScopedPointer<QnNoptixStyleAnimator> idleAnimator;
    QScopedPointer<QnNoptixStyleAnimator> stateAnimator;
    QPointer<QWidget> lastProxiedWidgetUnderMouse;
};
