#ifndef QN_ARCHIVE_DROP_INSTRUMENT_H
#define QN_ARCHIVE_DROP_INSTRUMENT_H

#include "instrument.h"

class QnDisplayController;

class ArchiveDropInstrument: public Instrument {
    Q_OBJECT;
public:
    ArchiveDropInstrument(QnDisplayController *controller, QObject *parent = NULL);

protected:
    virtual bool dragEnterEvent(QWidget *viewport, QDragEnterEvent *event) override;
    virtual bool dragMoveEvent(QWidget *viewport, QDragMoveEvent *event) override;
    virtual bool dragLeaveEvent(QWidget *viewport, QDragLeaveEvent *event) override;
    virtual bool dropEvent(QWidget *viewport, QDropEvent *event) override;

private:
    bool dropInternal(QGraphicsView *view, const QStringList &files, const QPoint &pos);

private:
    QStringList m_files;
    QnDisplayController *m_controller;
};

#endif // QN_ARCHIVE_DROP_INSTRUMENT_H
