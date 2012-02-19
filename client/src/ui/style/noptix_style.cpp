#include "noptix_style.h"
#include <QApplication>
#include <QPainter>
#include <QStyleOption>
#include <QMenu>
#include <QAction>
#include <utils/common/scoped_painter_rollback.h>
#include "skin.h"

// -------------------------------------------------------------------------- //
// QnNoptixStyle
// -------------------------------------------------------------------------- //
QnNoptixStyle::QnNoptixStyle(QStyle *style): 
    base_type(style)
{}

void QnNoptixStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const {
    switch (control) {
    case CC_Slider:
        if(drawSliderComplexControl(option, painter, widget))
            return;
        break;
    case CC_TitleBar:
        {
            /* Draw background in order to avoid painting artifacts. */
            QColor bgColor = option->palette.color(widget ? widget->backgroundRole() : QPalette::Window);
            if (bgColor != Qt::transparent) {
                bgColor.setAlpha(255);
                painter->fillRect(option->rect, bgColor);
            }
        }
        break;
    default:
        break;
    }

    base_type::drawComplexControl(control, option, painter, widget);
}

void QnNoptixStyle::drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const {
    switch(element) {
    case CE_MenuItem:
        if(drawMenuItemControl(option, painter, widget))
            return;
        break;
    default:
        break;
    }

    base_type::drawControl(element, option, painter, widget);
}

void QnNoptixStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const {
    switch(element) {
    case PE_IndicatorTabClose:
        if(drawTabClosePrimitive(option, painter, widget))
            return;
        break;
    default:
        break;
    }

    base_type::drawPrimitive(element, option, painter, widget);
}

int QnNoptixStyle::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *returnData) const {
    if (hint == QStyle::SH_ToolTipLabel_Opacity)
        return 255;

    return base_type::styleHint(hint, option, widget, returnData);
}

void QnNoptixStyle::polish(QApplication *application) {
    base_type::polish(application);

    QFont menuFont;
    menuFont.setFamily(QLatin1String("Bodoni MT"));
    menuFont.setPixelSize(18);
    application->setFont(menuFont, "QMenu");
}

void QnNoptixStyle::unpolish(QApplication *application) {
    application->setFont(QFont(), "QMenu");

    base_type::unpolish(application);
}

void QnNoptixStyle::polish(QWidget *widget) {
    base_type::polish(widget);
}

void QnNoptixStyle::unpolish(QWidget *widget) {
    base_type::unpolish(widget);
}

bool QnNoptixStyle::drawMenuItemControl(const QStyleOption *option, QPainter *painter, const QWidget *widget) const {
    const QStyleOptionMenuItem *itemOption = qstyleoption_cast<const QStyleOptionMenuItem *>(option);
    if(!itemOption)
        return false;
    
    const QMenu *menu = qobject_cast<const QMenu *>(widget);
    if(!menu) 
        return false;

    /* There are cases when we want an action to be checkable, but do not want the checkbox displayed in the menu. 
     * So we introduce an internal property for this. */
    QAction *action = menu->actionAt(option->rect.center());
    if(!action || !action->property(hideCheckBoxInMenuPropertyName).value<bool>()) 
        return false;
    
    QStyleOptionMenuItem::CheckType checkType = itemOption->checkType;
    QStyleOptionMenuItem *localOption = const_cast<QStyleOptionMenuItem *>(itemOption);
    localOption->checkType = QStyleOptionMenuItem::NotCheckable;
    base_type::drawControl(CE_MenuItem, option, painter, widget);
    localOption->checkType = checkType;
    return true;
}

bool QnNoptixStyle::drawSliderComplexControl(const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const {
    /* Bespin's slider painting is way to slow, so we do it our way. */
    const QStyleOptionSlider *sliderOption = qstyleoption_cast<const QStyleOptionSlider *>(option);
    if (!sliderOption) 
        return false;
    
    if (sliderOption->orientation != Qt::Horizontal) 
        return false; /* Non-horizontal sliders are not implemented. */
    
    const QRect grooveRect = subControlRect(CC_Slider, option, SC_SliderGroove, widget);
    QRect handleRect = subControlRect(CC_Slider, option, SC_SliderHandle, widget);
    handleRect.setSize(handleRect.size() * 2);
    handleRect.moveTopLeft(handleRect.topLeft() - QPoint(handleRect.width(), handleRect.height()) / 4);

    const bool hovered = (option->state & State_Enabled) && (option->activeSubControls & SC_SliderHandle);

    QPixmap grooveBorderPic = Skin::pixmap(QLatin1String("slider_groove_lborder.png"), grooveRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPixmap grooveBodyPic = Skin::pixmap(QLatin1String("slider_groove_body.png"), grooveRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPixmap handlePic = Skin::pixmap(hovered ? QLatin1String("slider_handle_active.png") : QLatin1String("slider_handle.png"), handleRect.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    painter->drawTiledPixmap(grooveRect.adjusted(grooveBorderPic.width(), 0, -grooveBorderPic.width(), 0), grooveBodyPic);
    painter->drawPixmap(grooveRect.topLeft(), grooveBorderPic);
    {
        QTransform oldTransform = painter->transform();
        painter->translate(grooveRect.left() + grooveRect.width(), grooveRect.top());
        painter->scale(-1.0, 1.0);
        painter->drawPixmap(0, 0, grooveBorderPic);
        painter->setTransform(oldTransform);
    }

    painter->drawPixmap(handleRect, handlePic);
    return true;
}

bool QnNoptixStyle::drawTabClosePrimitive(const QStyleOption *option, QPainter *painter, const QWidget *) const {
    bool sunken = option->state & State_Sunken;
    bool hover = (option->state & State_Enabled) && (option->state & State_MouseOver);
    if (sunken) 
        hover = false;

    QRectF rect = option->rect;
    qreal d = sunken ? 0.25 : (hover ? 0.1 : 0.2);
    rect.adjust(rect.width() * d, rect.height() * d, rect.width() * -d, rect.height() * -d);

    QBrush b = option->palette.text(); //Colors::mid( CCOLOR(tab.std, Bg), CCOLOR(tab.std, Fg), 3, 1+hover );

    QnScopedPainterAntialiasingRollback antialiasingRollback(painter, true);
    QnScopedPainterPenRollback penRollback(painter, QPen(b, qMin(rect.width(), rect.height()) * 0.2));
    painter->drawEllipse(rect);
    return true;

    return false;
}


