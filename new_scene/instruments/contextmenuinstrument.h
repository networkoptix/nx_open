#ifndef QN_CONTEXT_MENU_INSTRUMENT_H
#define QN_CONTEXT_MENU_INSTRUMENT_H

#include "instrument.h"
#include <QScopedPointer>
#include <QPoint>

class QAction;
class QMenu;

class ContextMenuInstrument : public Instrument {
    Q_OBJECT;
public:
    ContextMenuInstrument(QObject *parent);

    virtual ~ContextMenuInstrument();

protected:
    virtual bool mousePressEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool mouseMoveEvent(QWidget *viewport, QMouseEvent *event) override;
    virtual bool mouseReleaseEvent(QWidget *viewport, QMouseEvent *event) override;

private:
    QPoint m_mousePressPos;
    bool m_showMenu;
    QScopedPointer<QMenu> m_contextMenu;
};

#endif // QN_CONTEXT_MENU_INSTRUMENT_H
