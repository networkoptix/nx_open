#ifndef QN_DROP_INSTRUMENT_H
#define QN_DROP_INSTRUMENT_H

#include "instrument.h"
#include <core/resource/resource.h>

class QnWorkbenchController;

class DropInstrument: public Instrument {
    Q_OBJECT;
public:
    DropInstrument(QnWorkbenchController *controller, QObject *parent = NULL);

protected:
    virtual bool dragEnterEvent(QGraphicsScene *scene, QGraphicsSceneDragDropEvent *event) override;
    virtual bool dragMoveEvent(QGraphicsScene *scene, QGraphicsSceneDragDropEvent *event) override;
    virtual bool dragLeaveEvent(QGraphicsScene *scene, QGraphicsSceneDragDropEvent *event) override;
    virtual bool dropEvent(QGraphicsScene *scene, QGraphicsSceneDragDropEvent *event) override;

private:
    bool dropInternal(QGraphicsView *view, const QStringList &files, const QPoint &pos);

private:
    QStringList m_files;
    QnResourceList m_resources;
    QnWorkbenchController *const m_controller;
};

#endif // QN_DROP_INSTRUMENT_H
