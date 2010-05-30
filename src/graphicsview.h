#ifndef PH_graphicsview_h328
#define PH_graphicsview_h328

#include <QGraphicsView>
#include "./videodisplay/animation/scene_movement.h"

class GraphicsView: public QGraphicsView
{
	Q_OBJECT
public:
	GraphicsView();
	virtual ~GraphicsView();

	int zoom() const;

protected:
	virtual void wheelEvent ( QWheelEvent * e );
	virtual void keyPressEvent( QKeyEvent * e );
private slots:
	void onDragMoveAnimation(qreal value);
protected:
	int m_zoom;
	int m_xRotate;
	int m_yRotate;
	bool m_dragStarted;
	QPoint m_prevPos;
	QPoint m_centerPos;
	QPoint m_shift;
	QPoint m_firstPos;

	void updateTransform();

	
	void mouseReleaseEvent ( QMouseEvent * e);
	void mousePressEvent ( QMouseEvent * e);
	void mouseMoveEvent ( QMouseEvent * e);
	/**/

	struct GraphicsViewPriv;
	GraphicsViewPriv* d;

	CLSceneMovement m_movement;
	
};

#endif //PH_graphicsview_h328