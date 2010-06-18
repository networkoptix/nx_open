#ifndef item_zoom_h2118
#define item_zoom_h2118

#include "abstract_animation.h"
class QGraphicsItem;

class CLItemZoom : public CLAbstractAnimation
{
	Q_OBJECT
public:
	CLItemZoom(QGraphicsItem* item);
	virtual ~CLItemZoom();

	qreal zoomToscale(qreal zoom) const;
	qreal scaleTozoom(qreal scale) const;

	void zoom(qreal target_zoom, bool instatntly = false);
	

private:

	void zoom_helper();

private slots:
	void onNewFrame(int frame);
	//virtual void onFinished();

protected:

	
	QGraphicsItem* m_item;
	qreal m_zoom, m_start_point;

	qreal m_diff;



};

#endif //item_zoom_h2118