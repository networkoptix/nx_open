// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QCommonStyle>
#include <QtWidgets/QStyleOption>
#include <QtWidgets/private/qcommonstyle_p.h>

#include <qt_styles/style_base_private.h>

#include "../abstract_widget_animation.h"
#include "../helper.h"
#include "../style.h"

class QInputDialog;
class QScrollBar;
class QGraphicsWidget;

namespace nx::vms::client::desktop {

class StylePrivate: public StyleBasePrivate
{
    Q_DECLARE_PUBLIC(Style)

public:
    explicit StylePrivate();
    virtual ~StylePrivate() override;

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

    static bool isCheckableButton(const QStyleOption* option);

    /** TextButton is a PushButton which looks like a simple text (without bevel). */
    static bool isTextButton(const QStyleOption* option);

    /** Insert horizontal separator line into QInputDialog above its button box. */
    bool polishInputDialog(QInputDialog* inputDialog) const;

    /** Update QScrollArea hover if scrollBar is parented by one. */
    void updateScrollAreaHover(QScrollBar* scrollBar) const;

public:
    QScopedPointer<AbstractWidgetAnimation> idleAnimator;
    QPointer<QWidget> lastProxiedWidgetUnderMouse;
};

} // namespace nx::vms::client::desktop
