#ifndef QN_NOPTIX_STYLE_H
#define QN_NOPTIX_STYLE_H

#include <QtWidgets/QProxyStyle>
#include <QtWidgets/QStyleOptionProgressBar>

#include <common/common_globals.h>
#include <ui/graphics/items/standard/graphics_style.h>

class QnNoptixStyleAnimator;
class QnSkin;
class QnCustomizer;

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
    bool skipItemViewPrimitive(PrimitiveElement element, const QStyleOption* option, QPainter* painter, const QWidget* widget) const;

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
    QnCustomizer *m_customizer;
};


/**
 * Name of a property to set on a <tt>QAction</tt> that is checkable, but for
 * which checkbox must not be displayed in a menu.
 */
#define HideCheckBoxInMenu              _id("_qn_hideCheckBoxInMenu")

/**
 * Name of a property to set on a <tt>QTreeView</tt> to make it hide
 * last row if there is not enough space for it.
 */
#define HideLastRowInTreeIfNotEnoughSpace _id("_qn_hideLastRowInTreeIfNotEnoughSpace")


#endif // QN_NOPTIX_STYLE_H
