#ifndef QN_NOPTIX_STYLE_H
#define QN_NOPTIX_STYLE_H

#include <QtWidgets/QProxyStyle>
#include <QtWidgets/QStyleOptionProgressBar>

#include <ui/graphics/items/standard/graphics_style.h>
#include <common/config.h>


class QnNoptixStyleAnimator;
class QnSkin;
class QnGlobals;
class QnCustomizer;

class QnStyleOptionProgressBar: public QStyleOptionProgressBarV2 {
public:
    enum StyleOptionVersion { Version = 3 };

    QnStyleOptionProgressBar():
        QStyleOptionProgressBar(Version)
    {}

    QList<int> separators;
};

class QnNoptixStyle: public QProxyStyle, public GraphicsStyle {
    Q_OBJECT;

    typedef QProxyStyle base_type;

public:
    QnNoptixStyle(QStyle *style = NULL);
    virtual ~QnNoptixStyle();

    virtual QPixmap generatedIconPixmap(QIcon::Mode iconMode, const QPixmap &pixmap, const QStyleOption *option) const override;

    virtual void drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget = 0) const override;
    virtual void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = 0) const override;
    virtual void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = 0) const override;
    virtual int pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const override;
    virtual QRect subControlRect(ComplexControl control, const QStyleOptionComplex *option, SubControl subControl, const QWidget *widget) const override;

    virtual int styleHint(StyleHint hint, const QStyleOption *option = 0, const QWidget *widget = 0, QStyleHintReturn *returnData = 0) const override;

    virtual void polish(QApplication *application) override;
    virtual void unpolish(QApplication *application) override;
    virtual void polish(QWidget *widget) override;
    virtual void unpolish(QWidget *widget) override;
    virtual void polish(QGraphicsWidget *widget) override;
    virtual void unpolish(QGraphicsWidget *widget) override;

    using base_type::subControlRect;
    using base_type::sliderPositionFromValue;
    using base_type::sliderValueFromPosition;

protected:
    bool drawMenuItemControl(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    bool drawItemViewItemControl(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    bool drawProgressBarControl(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    bool drawSliderComplexControl(const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const;
    bool drawToolButtonComplexControl(const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const;
    bool drawTabClosePrimitive(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    bool drawBranchPrimitive(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    bool drawPanelItemViewPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const;

    bool scrollBarSubControlRect(const QStyleOptionComplex *option, SubControl subControl, const QWidget *widget, QRect *result) const;

private:
    void setHoverProgress(const QWidget *widget, qreal value) const;
    qreal hoverProgress(const QStyleOption *option, const QWidget *widget, qreal speed) const;
    void stopHoverTracking(const QWidget *widget) const;
    void drawProgressBarGroove(const QStyleOptionProgressBarV2 *pb, QPainter *painter, const QWidget *widget) const;
    void drawProgressBarContents(const QStyleOptionProgressBarV2 *pb, QPainter *painter, const QWidget *widget) const;
    void drawProgressBarLabel(const QStyleOptionProgressBarV2 *pb, QPainter *painter, const QWidget *widget) const;

private:
    QnNoptixStyleAnimator *m_hoverAnimator, *m_rotationAnimator;
    QnSkin *m_skin;
    QnGlobals *m_globals;
    QnCustomizer *m_customizer;
    QIcon m_branchClosed, m_branchOpen, m_closeTab;
    QPixmap m_grooveBorder, m_grooveBody, m_sliderHandleHovered, m_sliderHandle;
};


/**
 * Name of a property to set on a <tt>QAction</tt> that is checkable, but for
 * which checkbox must not be displayed in a menu. 
 */
#define HideCheckBoxInMenu              _id("_qn_hideCheckBoxInMenu")

/**
 * Name of a property to set on a <tt>QAbstractItemView</tt> to change
 * default opacity if its items' background.
 */
#define ItemViewItemBackgroundOpacity   _id("_qn_itemViewItemBackgroundOpacity")

/**
 * Name of a property to set on a <tt>QAbstractSlider</tt> or 
 * <tt>AbstractGraphicsSlider</tt> to change default width of the slider handle.
 */
#define SliderLength                    _id("_qn_sliderLength")

/**
 * Name of a property to set on a <tt>QToolButton</tt> to make it rotate
 * when checked. Value defines rotation speed in degrees per second.
 */
#define ToolButtonCheckedRotationSpeed  _id("_qn_toolButtonCheckedRotationSpeed")

/**
 * Name of a property to set on a <tt>QTreeView</tt> to make it hide
 * last row if there is not enough space for it.
 */
#define HideLastRowInTreeIfNotEnoughSpace _id("_qn_hideLastRowInTreeIfNotEnoughSpace")


#endif // QN_NOPTIX_STYLE_H
