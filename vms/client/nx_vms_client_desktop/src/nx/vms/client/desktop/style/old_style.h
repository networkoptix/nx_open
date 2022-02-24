// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtWidgets/QProxyStyle>
#include <QtWidgets/QStyleOptionProgressBar>

#include <common/common_globals.h>

namespace nx::vms::client::desktop {

class AbstractWidgetAnimation;
class Skin;

/**
 * Old style implementation used in versions 1.0 - 2.6.
 */
class OldStyle: public QProxyStyle
{
    Q_OBJECT
    using base_type = QProxyStyle;

public:
    /**
    * Name of a property to set on a <tt>QAction</tt> that is checkable, but for which checkbox
    * must not be displayed in a menu.
    */
    static constexpr auto kHideCheckBoxInMenu = "_qn_hideCheckBoxInMenu";

    OldStyle(QStyle* style = nullptr);

    virtual QPixmap generatedIconPixmap(
        QIcon::Mode iconMode, const QPixmap& pixmap, const QStyleOption* option) const override;

    virtual void drawComplexControl(ComplexControl control,
        const QStyleOptionComplex* option,
        QPainter* painter,
        const QWidget* widget = nullptr) const override;
    virtual void drawControl(ControlElement element,
        const QStyleOption* option,
        QPainter* painter,
        const QWidget* widget = nullptr) const override;
    virtual void drawPrimitive(PrimitiveElement element,
        const QStyleOption* option,
        QPainter* painter,
        const QWidget* widget = nullptr) const override;

    virtual int styleHint(StyleHint hint,
        const QStyleOption* option = nullptr,
        const QWidget* widget = nullptr,
        QStyleHintReturn* returnData = nullptr) const override;

    virtual void polish(QApplication* application) override;
    virtual void polish(QWidget* widget) override;

protected:
    bool drawMenuItemControl(
        const QStyleOption* option, QPainter* painter, const QWidget* widget) const;
    bool drawItemViewItemControl(
        const QStyleOption* option, QPainter* painter, const QWidget* widget) const;

private:
    void setHoverProgress(const QWidget* widget, qreal value) const;
    qreal hoverProgress(const QStyleOption* option, const QWidget* widget, qreal speed) const;
    void stopHoverTracking(const QWidget* widget) const;

private:
    AbstractWidgetAnimation* m_hoverAnimator;
    Skin* m_skin;
};

} // namespace nx::vms::client::desktop
