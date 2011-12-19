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
    virtual bool dragEnterEvent(QWidget *viewport, QDragEnterEvent *event) override;
    virtual bool dragMoveEvent(QWidget *viewport, QDragMoveEvent *event) override;
    virtual bool dragLeaveEvent(QWidget *viewport, QDragLeaveEvent *event) override;
    virtual bool dropEvent(QWidget *viewport, QDropEvent *event) override;

private:
    bool dropInternal(QGraphicsView *view, const QStringList &files, const QPoint &pos);

private:
    QStringList m_files;
    QnResourceList m_resources;
    QnWorkbenchController *const m_controller;
};

#endif // QN_DROP_INSTRUMENT_H
