#ifndef QN_PTZ_INSTRUMENT_H
#define QN_PTZ_INSTRUMENT_H

#include <QtCore/QWeakPointer>
#include <QtCore/QBasicTimer>
#include <QtCore/QVector>
#include <QtGui/QVector3D>

#include <core/resource/resource_fwd.h>
#include <api/api_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include "drag_processing_instrument.h"

class SelectionItem;
class PtzSplashItem;
class PtzSelectionItem;
class PtzArrowItem;

class QnWorkbenchPtzController;
class QnMediaResourceWidget;

class PtzInstrument: public DragProcessingInstrument, public QnWorkbenchContextAware {
    Q_OBJECT;

    typedef DragProcessingInstrument base_type;
public:
    PtzInstrument(QObject *parent = NULL);
    virtual ~PtzInstrument();

    qreal ptzItemZValue() const {
        return m_ptzItemZValue;
    }

    void setPtzItemZValue(qreal ptzItemZValue);

signals:
    void ptzProcessStarted(QnMediaResourceWidget *widget);
    void ptzStarted(QnMediaResourceWidget *widget);
    void ptzFinished(QnMediaResourceWidget *widget);
    void ptzProcessFinished(QnMediaResourceWidget *widget);

protected:
    virtual void installedNotify() override;
    virtual void aboutToBeDisabledNotify() override;
    virtual void aboutToBeUninstalledNotify() override;
    virtual bool registeredNotify(QGraphicsItem *item) override;
    virtual void unregisteredNotify(QGraphicsItem *item) override;

    virtual void timerEvent(QTimerEvent *event) override;

    virtual bool animationEvent(AnimationEvent *event) override;

    virtual bool mousePressEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool mouseDoubleClickEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) override;
    virtual bool mousePressEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) override;

    virtual void startDragProcess(DragInfo *info) override;
    virtual void startDrag(DragInfo *info) override;
    virtual void dragMove(DragInfo *info) override;
    virtual void finishDrag(DragInfo *info) override;
    virtual void finishDragProcess(DragInfo *info) override;

private slots:
    void at_target_optionsChanged();
    void at_splashItem_destroyed();

private:
    QnMediaResourceWidget *target() const {
        return m_target.data();
    }

    void setTarget(QnMediaResourceWidget *target);
    
    PtzSplashItem *newSplashItem(QGraphicsItem *parentItem);

    PtzSelectionItem *selectionItem() const {
        return m_selectionItem.data();
    }

    PtzArrowItem *arrowItem() const {
        return m_arrowItem.data();
    }

    void ensureSelectionItem();
    void ensureArrowItem();

    void ptzMoveTo(QnMediaResourceWidget *widget, const QPointF &pos);
    void ptzMoveTo(QnMediaResourceWidget *widget, const QRectF &rect);
    void ptzUnzoom(QnMediaResourceWidget *widget);

private:
    QnWorkbenchPtzController *m_ptzController;

    int m_clickDelayMSec;
    qreal m_ptzItemZValue;
    qreal m_expansionSpeed;

    QWeakPointer<PtzSelectionItem> m_selectionItem;
    QWeakPointer<PtzArrowItem> m_arrowItem;
    QWeakPointer<QWidget> m_viewport;
    QWeakPointer<QnMediaResourceWidget> m_target;

    bool m_isClick;
    bool m_isDoubleClick;
    bool m_ptzStartedEmitted;

    QBasicTimer m_clickTimer;
    QPointF m_clickPos;

    struct SplashItemAnimation {
        SplashItemAnimation(): item(NULL), fadingIn(true), expansionMultiplier(0.0), opacityMultiplier(0.0) {}
        SplashItemAnimation(PtzSplashItem *item): item(item), fadingIn(true), expansionMultiplier(0.0), opacityMultiplier(0.0) {}
        SplashItemAnimation(PtzSplashItem *item, qreal expansionMultiplier, qreal opacityMultiplier): item(item), fadingIn(true), expansionMultiplier(expansionMultiplier), opacityMultiplier(opacityMultiplier) {}

        PtzSplashItem *item;
        bool fadingIn;
        qreal expansionMultiplier; /**< Expansion speed relative to standard expansion speed. */
        qreal opacityMultiplier;

        friend bool operator==(const SplashItemAnimation &l, const SplashItemAnimation &r) { return l.item == r.item; }
    };

    QList<SplashItemAnimation> m_freeAnimations, m_activeAnimations;
};


#endif // QN_PTZ_INSTRUMENT_H
