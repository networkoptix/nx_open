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
    QnPaletteColor mainColor(style::Colors::Palette palette) const;

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

public:
    QnGenericPalette palette;
    QnNoptixStyleAnimator *idleAnimator;
    QnNoptixStyleAnimator *stateAnimator;
};
