#ifndef QN_CLICK_INSTRUMENT_H
#define QN_CLICK_INSTRUMENT_H

#include "instrument.h"
#include <QPoint>

class ClickInstrument;

namespace detail {
    class ClickInstrumentHandler {
    public:
        ClickInstrumentHandler(ClickInstrument *instrument): m_instrument(instrument), m_isClick(false), m_isDoubleClick(false) {}

        bool mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event);
        bool mouseMoveEvent(QGraphicsSceneMouseEvent *event);
        bool mousePressEvent(QGraphicsSceneMouseEvent *event);
        bool mouseReleaseEvent(QGraphicsItem *item, QGraphicsScene *scene, QGraphicsSceneMouseEvent *event);

    private:
        ClickInstrument *m_instrument;
        QPoint m_mousePressPos;
        bool m_isClick;
        bool m_isDoubleClick;
    };

} // namespace detail


class ClickInstrument: public Instrument {
    Q_OBJECT;
public:
    /**
     * \param watchedType               Type of click events that this instrument will watch.
     *                                  Note that only SCENE and ITEM types are supported.
     * \param parent                    Parent object for this instrument.
     */
    ClickInstrument(WatchedType watchedType, QObject *parent = NULL);

signals:
    void clicked(QGraphicsView *view, QGraphicsItem *item);
    void doubleClicked(QGraphicsView *view, QGraphicsItem *item);

    void clicked(QGraphicsView *view);
    void doubleClicked(QGraphicsView *view);

protected:
    virtual bool mouseDoubleClickEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) override;
    virtual bool mouseMoveEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) override;
    virtual bool mousePressEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) override;
    virtual bool mouseReleaseEvent(QGraphicsScene *scene, QGraphicsSceneMouseEvent *event) override;

    virtual bool mouseDoubleClickEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) override;
    virtual bool mouseMoveEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) override;
    virtual bool mousePressEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) override;
    virtual bool mouseReleaseEvent(QGraphicsItem *item, QGraphicsSceneMouseEvent *event) override;

    virtual bool isWillingToWatch(QGraphicsItem *) const override { return true; }

private:
    friend class detail::ClickInstrumentHandler;

    detail::ClickInstrumentHandler m_handler;
};

#endif // QN_CLICK_INSTRUMENT_H
