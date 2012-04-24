#ifndef QN_NOPTIX_STYLE_H
#define QN_NOPTIX_STYLE_H

#include <QProxyStyle>
#include <ui/graphics/items/standard/graphics_style.h>

class QnNoptixStyleAnimator;
class QnSkin;

class QnNoptixStyle: public QProxyStyle, public GraphicsStyle { 
    Q_OBJECT;

    typedef QProxyStyle base_type;

public:
    QnNoptixStyle(QStyle *style = NULL);
    virtual ~QnNoptixStyle();

    virtual void drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget = 0) const override;
    virtual void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = 0) const override;
    virtual void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = 0) const override;
    virtual int pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const override;

    virtual int styleHint(StyleHint hint, const QStyleOption *option = 0, const QWidget *widget = 0, QStyleHintReturn *returnData = 0) const override;

    virtual void polish(QApplication *application) override;
    virtual void unpolish(QApplication *application) override;
    virtual void polish(QWidget *widget) override;
    virtual void unpolish(QWidget *widget) override;

    using base_type::subControlRect;

protected:
    bool drawMenuItemControl(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    bool drawItemViewItemControl(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    bool drawSliderComplexControl(const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const;
    bool drawToolButtonComplexControl(const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const;
    bool drawTabClosePrimitive(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    bool drawBranchPrimitive(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    bool drawPanelItemViewPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const;

private:
    void setHoverProgress(const QWidget *widget, qreal value) const;
    qreal hoverProgress(const QStyleOption *option, const QWidget *widget, qreal speed) const;
    void stopHoverTracking(const QWidget *widget) const;

private:
    QnNoptixStyleAnimator *m_animator;
    QnSkin *m_skin;
    QIcon m_branchClosed, m_branchOpen, m_closeTab;
    QPixmap m_grooveBorder, m_grooveBody, m_sliderHandleHovered, m_sliderHandle;
};


namespace Qn { namespace {
    const char *HideCheckBoxInMenu              = "_qn_hideCheckBoxInMenu";
    const char *ItemViewItemBackgroundOpacity   = "_qn_itemViewItemBackgroundOpacity";
    const char *SliderLength                    = "_qn_sliderLength";

#define HideCheckBoxInMenu HideCheckBoxInMenu
#define ItemViewItemBackgroundOpacity ItemViewItemBackgroundOpacity
#define SliderLength SliderLength
}}

#endif // QN_NOPTIX_STYLE_H
