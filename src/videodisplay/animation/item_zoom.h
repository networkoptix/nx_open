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

	void zoom(int target_zoom);

private slots:
	void onNewFrame(int frame);
	//virtual void onFinished();

protected:
	
	QGraphicsItem* m_item;
	int m_zoom, m_start_point;

	int m_diff;



};

#endif //item_zoom_h2118