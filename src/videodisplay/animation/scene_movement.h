#ifndef scene_movement_h1123
#define scene_movement_h1123

#include "animation_timeline.h"
#include <QPoint>

class QGraphicsView;

class CLSceneMovement : public QObject
{
	Q_OBJECT
public:
	CLSceneMovement(QGraphicsView* gview);
	virtual ~CLSceneMovement();

	void move (QPointF dest);
	void stop();

private slots:
		void onNewPos(int pos);

protected:

	QGraphicsView* m_view;
	CLAnimationTimeLine m_timeline;

	QPointF m_startpoint;
	QPointF m_delta;


};

#endif //scene_movement_h1123