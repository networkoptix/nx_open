#ifndef item_zoom_h2118
#define item_zoom_h2118

#include "animation.h"
class QGraphicsItem;

class CLItemTransform : public CLAnimation
{
    Q_OBJECT
    Q_PROPERTY(qreal zoom	READ getZoom WRITE setZoom)
    Q_PROPERTY(qreal rotation	READ getRotation WRITE setRotation)
public:
	CLItemTransform(QGraphicsItem* item, GraphicsView* gview);
	virtual ~CLItemTransform();

	qreal zoomToscale(qreal zoom) const;
	qreal scaleTozoom(qreal scale) const;

	void zoom_abs(qreal target_zoom, int duration, int delay);
	void z_rotate_delta(const QPointF &center, qreal angle, int duration, int delay);
	void z_rotate_abs(const QPointF &center, qreal angle, int duration, int delay );
	void move_abs(QPointF pos, int duration, int delay);

	qreal getRotation() const;
	qreal getZoom() const;

    void setRotation(qreal r);
    void setZoom(qreal z);


private:

	void transform_helper();

protected:

	QPointF m_rotatePoint;

	QGraphicsItem* m_item;

	qreal m_zoom;
	qreal m_Zrotation;

};

#endif //item_zoom_h2118
