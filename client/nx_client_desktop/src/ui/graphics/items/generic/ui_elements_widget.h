#pragma once

#include <QtCore/QPointer>

#include <QtWidgets/QGraphicsWidget>

#include <ui/graphics/items/standard/graphics_widget.h>

class QGraphicsView;
class Instrument;

/**
 * A graphics widget that has the same coordinate system as a viewport.
 *
 * Useful for placing UI controls.
 */
class QnUiElementsWidget: public GraphicsWidget {
    Q_OBJECT;
    typedef GraphicsWidget base_type;

public:
    QnUiElementsWidget(QGraphicsItem *parent = NULL, Qt::WindowFlags windowFlags = 0);
    virtual ~QnUiElementsWidget();

protected:
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private slots:
    void updateTransform(QGraphicsView *view);
    void updateSize(QGraphicsView *view);

private:
    QPointer<Instrument> m_instrument;
};
