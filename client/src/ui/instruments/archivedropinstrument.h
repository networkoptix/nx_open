#ifndef QN_ARCHIVE_DROP_INSTRUMENT_H
#define QN_ARCHIVE_DROP_INSTRUMENT_H

#include "instrument.h"

class QnDisplayState;

class ArchiveDropInstrument: public Instrument {
    Q_OBJECT;
public:
    ArchiveDropInstrument(QnDisplayState *state, QObject *parent = NULL);

protected:
    virtual bool dragEnterEvent(QWidget *viewport, QDragEnterEvent *event) override;
    virtual bool dragMoveEvent(QWidget *viewport, QDragMoveEvent *event) override;
    virtual bool dragLeaveEvent(QWidget *viewport, QDragLeaveEvent *event) override;
    virtual bool dropEvent(QWidget *viewport, QDropEvent *event) override;

    void findAcceptedFiles(const QString &path);

private:
    QStringList m_files;
    QnDisplayState *m_state;
};

#endif // QN_ARCHIVE_DROP_INSTRUMENT_H
