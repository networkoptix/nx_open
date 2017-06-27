#ifndef QN_GRAPHICS_VIEW_H
#define QN_GRAPHICS_VIEW_H

#include <QtWidgets/QGraphicsView>

//#define QN_GRAPHICS_VIEW_DEBUG_PERFORMANCE

class QnGraphicsView;

// TODO: #Elric get rid of layer painters, use items!

/**
 * Base class for detached graphics scene layer painters.
 */
class QnLayerPainter
{
public:
    QnLayerPainter();

    virtual ~QnLayerPainter();

    virtual void drawLayer(QPainter *painter, const QRectF &rect) = 0;

    bool isEnabled() const;
    void setEnabled(bool enabled);

protected:
    virtual void installedNotify() {}

    virtual void aboutToBeUninstalledNotify() {}

    void ensureUninstalled();

    QnGraphicsView *view() const {
        return m_view;
    }

    QGraphicsScene::SceneLayer layer() const {
        return m_layer;
    }

private:
    friend class QnGraphicsView;

    /** View that this painter is installed to. */
    QnGraphicsView *m_view;

    /** Scene layer that this painter draws. */
    QGraphicsScene::SceneLayer m_layer;

    /** Flag that painter is enabled. */
    bool m_enabled;
};


/**
 * Graphics view that has more hooks than the one provided by Qt.
 */
class QnGraphicsView: public QGraphicsView {
    Q_OBJECT;
    Q_FLAGS(PaintFlags PaintFlag);

    typedef QGraphicsView base_type;

public:
    enum PaintFlag {
        /**< Invoke inherited implementation when drawing background. */
        PaintInheritedBackround = 0x1,  

        /**< Invoke inherited implementation when drawing foreground. */
        PaintInheritedForeground = 0x2, 

        /**< Invoke base paint implementation even if painting on some
         * external surface (i.e. not current viewport).
         * This can happen as a result of <tt>grabWidget()</tt> call. */
        PaintOnExternalSurfaces = 0x4, 
    };
    Q_DECLARE_FLAGS(PaintFlags, PaintFlag);

    enum BehaviorFlag {
        CenterOnShow = 0x1,
        InvokeInheritedMouseWheel = 0x2
    };
    Q_DECLARE_FLAGS(BehaviorFlags, BehaviorFlag);

    QnGraphicsView(QGraphicsScene *scene, QWidget * parent = NULL);

    virtual ~QnGraphicsView();

    PaintFlags paintFlags() const {
        return m_paintFlags;
    }

    void setPaintFlags(PaintFlags paintFlags);

    BehaviorFlags behaviorFlags() const {
        return m_behaviorFlags;
    }

    void setBehaviorFlags(BehaviorFlags behaviorFlags);

    void installLayerPainter(QnLayerPainter *painter, QGraphicsScene::SceneLayer layer);

    void uninstallLayerPainter(QnLayerPainter *painter);

protected:
    virtual void showEvent(QShowEvent *event) override;
    virtual void paintEvent(QPaintEvent *event) override;
    virtual void wheelEvent(QWheelEvent *event) override;
    virtual void drawBackground(QPainter *painter, const QRectF &rect) override;
    virtual void drawForeground(QPainter *painter, const QRectF &rect) override;

private:
    bool isInRedirectedPaint() const;

private:
    PaintFlags m_paintFlags;
    BehaviorFlags m_behaviorFlags;
    QList<QnLayerPainter *> m_foregroundPainters;
    QList<QnLayerPainter *> m_backgroundPainters;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnGraphicsView::PaintFlags);
Q_DECLARE_OPERATORS_FOR_FLAGS(QnGraphicsView::BehaviorFlags);

#endif // QN_GRAPHICS_VIEW_H

    