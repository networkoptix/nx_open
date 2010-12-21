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

	void stop();

	qreal zoomToscale(qreal zoom) const;
	qreal scaleTozoom(qreal scale) const;

	void zoom_abs(qreal target_zoom, int duration, int delay);
	void z_rotate_delta(QPointF center, qreal angle, int duration, int delay);
	void z_rotate_abs(QPointF center, qreal angle, int duration, int delay );
	void move_abs(QPointF pos, int duration, int delay);

	qreal current_zrotation() const;
	qreal current_zoom() const;


private:

	void transform_helper();

private slots:
	void valueChanged ( qreal pos );



	virtual void onFinished();

protected:

	QPointF m_rotatePoint;

	
	QGraphicsItem* m_item;

	CLAnimationUnit<qreal> m_zoom;
	CLAnimationUnit<qreal> m_Zrotation;
	CLAnimationUnit<QPointF> m_movment;

	bool m_zooming, m_rotating, m_moving;

};

#endif //item_zoom_h2118