#pragma once

#include <QtWidgets/QCommonStyle>
#include <QtWidgets/QStyleOption>
#include <private/qcommonstyle_p.h>

#include <ui/common/geometry.h>

#include "nx_style.h"
#include "generic_palette.h"
#include "noptix_style_animator.h"
#include "helper.h"

class QnNxStylePrivate : public QCommonStylePrivate, public QnGeometry
{
    Q_DECLARE_PUBLIC(QnNxStyle)

public:
    QnNxStylePrivate();

    QnPaletteColor findColor(const QColor &color) const;
    QnPaletteColor mainColor(QnNxStyle::Colors::Palette palette) const;

    QColor checkBoxColor(const QStyleOption *option, bool radio = false) const;

    void drawSwitch(
            QPainter *painter,
            const QStyleOption *option,
            const QWidget *widget = nullptr) const;

    void drawCheckBox(
            QPainter *painter,
            const QStyleOption *option,
            const QWidget *widget = nullptr) const;

    void drawRadioButton(
            QPainter *painter,
            const QStyleOption *option,
            const QWidget *widget = nullptr) const;

    void drawSortIndicator(
            QPainter *painter,
            const QStyleOption *option,
            const QWidget *widget = nullptr) const;

    void drawCross(
            QPainter* painter,
            const QRect& rect,
            const QColor& color) const;

    /* Insert horizontal separator line into QInputDialog above its button box. */
    bool polishInputDialog(QInputDialog* inputDialog) const;

public:
    QnGenericPalette palette;
    QnNoptixStyleAnimator *idleAnimator;
    QnNoptixStyleAnimator *stateAnimator;
};
