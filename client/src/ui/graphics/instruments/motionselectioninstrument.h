#ifndef QN_MOTION_SELECTION_INSTRUMENT_H
#define QN_MOTION_SELECTION_INSTRUMENT_H

#include "dragprocessinginstrument.h"
#include <QWeakPointer>

class QnResourceWidget;
class MotionSelectionItem;

class MotionSelectionInstrument: public DragProcessingInstrument {
    Q_OBJECT;

    typedef DragProcessingInstrument base_type;

public:
    MotionSelectionInstrument(QObject *parent = NULL);
    
    virtual ~MotionSelectionInstrument();

signals:
    void selectionProcessStarted(QGraphicsView *view, QnResourceWidget *widget);
    void selectionStarted(QGraphicsView *view, QnResourceWidget *widget);
    void selectionFinished(QGraphicsView *view, QnResourceWidget *widget);
    void selectionProcessFinished(QGraphicsView *view, QnResourceWidget *widget);

protected:
    virtual void installedNotify() override;
    virtual void aboutToBeUninstalledNotify() override;

    virtual bool mousePressEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool paintEvent(QWidget *viewport, QPaintEvent *event) override;

    virtual void startDragProcess(DragInfo *info) override;
    virtual void startDrag(DragInfo *info) override;
    virtual void dragMove(DragInfo *info) override;
    virtual void finishDrag(DragInfo *info) override;
    virtual void finishDragProcess(DragInfo *info) override;

    MotionSelectionItem *selectionItem() const {
        return m_selectionItem.data();
    }

    QnResourceWidget *target() const {
        return m_target.data();
    }

private:
    QWeakPointer<MotionSelectionItem> m_selectionItem;
    QWeakPointer<QnResourceWidget> m_target;
    bool m_selectionStartedEmitted;
};


#endif // QN_MOTION_SELECTION_INSTRUMENT_H
