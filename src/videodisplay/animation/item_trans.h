#ifndef item_zoom_h2118
#define item_zoom_h2118

#include <QPointF>
#include "abstract_animation.h"
class QGraphicsItem;

class CLItemTransform : public CLAbstractAnimation
{
	Q_OBJECT
public:
	CLItemTransform(QGraphicsItem* item);
	virtual ~CLItemTransform();

	qreal zoomToscale(qreal zoom) const;
	qreal scaleTozoom(qreal scale) const;

	void zoom(qreal target_zoom, bool instantly = false);


	void z_rotate_delta(QPointF center, qreal angle, bool instantly = false);
	void z_rotate_abs(QPointF center, qreal angle, bool instantly = false);
	

private:

	void transform_helper();

private slots:
	void onNewFrame(int frame);



	virtual void onFinished();

protected:

	QPointF m_rotatePoint;

	
	QGraphicsItem* m_item;

	CLAnimationUnit m_zoom;
	CLAnimationUnit m_Zrotation;

	bool m_zooming, m_rotating;

};

#endif //item_zoom_h2118