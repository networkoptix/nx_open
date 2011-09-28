#ifndef scene_movement_h1123
#define scene_movement_h1123

#include "animation.h"

class GraphicsView;

class CLSceneMovement : public CLAnimation
{
	Q_OBJECT
    Q_PROPERTY(QPointF position	READ getPosition WRITE setPosition)
public:
	CLSceneMovement(GraphicsView* gview);
	virtual ~CLSceneMovement();

	void move (QPointF dest, int duration, int delay, CLAnimationCurve curve =  SLOW_END_POW_40 );
	void move (int dx, int dy, int duration, bool limited , int delay, CLAnimationCurve curve =  SLOW_END_POW_40 );

    QPointF getPosition() const;
    void setPosition(QPointF p);
    
protected:

	GraphicsView* m_view;
	bool m_limited;

    QPointF m_startpoint;
    QPointF m_delta;

};

#endif //scene_movement_h1123
