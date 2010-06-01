#ifndef scene_movement_h1123
#define scene_movement_h1123

#include "abstract_animation.h"
#include <QPoint>

class QGraphicsView;

class CLSceneMovement : public CLAbstractAnimation
{
	Q_OBJECT
public:
	CLSceneMovement(QGraphicsView* gview);
	virtual ~CLSceneMovement();

	void move (QPointF dest);


private slots:
	virtual void onNewFrame(int frame);

protected:

	QGraphicsView* m_view;

	QPointF m_startpoint;
	QPointF m_delta;


};

#endif //scene_movement_h1123