#ifndef scene_movement_h1123
#define scene_movement_h1123

#include "abstract_animation.h"
#include <QPoint>

class GraphicsView;

class CLSceneMovement : public CLAbstractAnimation
{
	Q_OBJECT
public:
	CLSceneMovement(GraphicsView* gview);
	virtual ~CLSceneMovement();

	void move (QPointF dest, int duration);
	void move (int dx, int dy, int duration, bool limited = false);


private slots:
	void valueChanged ( qreal pos );

protected:

	GraphicsView* m_view;

	QPointF m_startpoint;
	QPointF m_delta;

	bool m_limited;

};

#endif //scene_movement_h1123