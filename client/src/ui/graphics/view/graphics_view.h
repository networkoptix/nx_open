#ifndef QN_GRAPHICS_VIEW_H
#define QN_GRAPHICS_VIEW_H

#include <QGraphicsView>

class QnGraphicsView;

/**
 * Base class for detached graphics scene layer painters.
 */
class QnLayerPainter
{
public:
    QnLayerPainter();

    virtual ~QnLayerPainter();

    virtual void drawLayer(QPainter *painter, const QRectF &rect) = 0;

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
        BACKGROUND_DONT_INVOKE_BASE = 0x1, /**< Don't invoke inherited implementation when drawing background. */
        FOREGROUND_DONT_INVOKE_BASE = 0x2  /**< Don't invoke inherited implementation when drawing foreground. */
    };
    Q_DECLARE_FLAGS(PaintFlags, PaintFlag);

    QnGraphicsView(QGraphicsScene *scene, QWidget * parent = NULL);

    virtual ~QnGraphicsView();

    PaintFlags paintFlags() const {
        return m_paintFlags;
    }

    void setPaintFlags(PaintFlags paintFlags);

    void installLayerPainter(QnLayerPainter *painter, QGraphicsScene::SceneLayer layer);

    void uninstallLayerPainter(QnLayerPainter *painter);

protected:
    virtual void drawBackground(QPainter *painter, const QRectF &rect) override;
    virtual void drawForeground(QPainter *painter, const QRectF &rect) override;

private:
    PaintFlags m_paintFlags;
    QList<QnLayerPainter *> m_foregroundPainters;
    QList<QnLayerPainter *> m_backgroundPainters;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnGraphicsView::PaintFlags)

#endif // QN_GRAPHICS_VIEW_H
