#ifndef QN_TOOL_TIP_INSTRUMENT_H
#define QN_TOOL_TIP_INSTRUMENT_H

#include "instrument.h"

/**
 * Instrument that implements tooltips that are shown on the scene as graphics items.
 * This approach is better than the default one because showing widgets on top of
 * OpenGL window (graphics view in our case) results in FPS drop.
 */
class ToolTipInstrument: public Instrument {
    Q_OBJECT

    typedef Instrument base_type;

public:
    ToolTipInstrument(QObject *parent);
    virtual ~ToolTipInstrument();

    void addIgnoredItem(QGraphicsItem* item);
    bool removeIgnoredItem(QGraphicsItem* item);

protected:
    virtual bool event(QWidget* viewport, QEvent* event) override;

private:
    QString widgetToolTip(QWidget* idget, const QPoint& pos) const;
    QString itemToolTip(QGraphicsItem* item, const QPointF& pos) const;

    void at_ignoredWidgetDestroyed();

private:
    QSet<QGraphicsItem*> m_ignoredItems;
};

#endif // QN_TOOL_TIP_INSTRUMENT_H
