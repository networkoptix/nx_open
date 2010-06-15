#ifndef PH_graphicsview_h328
#define PH_graphicsview_h328

#include <QGraphicsView>
#include "./videodisplay/animation/scene_movement.h"
#include "./videodisplay/animation/scene_zoom.h"
#include "./videodisplay/animation/mouse_state.h"

class VideoWindow;
class VideoCamerasLayout;

class GraphicsView: public QGraphicsView
{
	Q_OBJECT
public:
	GraphicsView();
	virtual ~GraphicsView();

	void zoomDefault();
	void setRealSceneRect(QRect rect);
	QRect getRealSceneRect() const;

	void setCamLayOut(const VideoCamerasLayout *);

	VideoWindow* getSelectedWnd() const;

	void setZeroSelection(); 


protected:
	virtual void wheelEvent ( QWheelEvent * e );
	virtual void keyPressEvent( QKeyEvent * e );


	void onNewItemSelected_helper(VideoWindow* new_wnd);

private slots:

protected:
	
	int m_xRotate;
	int m_yRotate;
	
	void updateTransform();

	
	void mouseReleaseEvent ( QMouseEvent * e);
	void mousePressEvent ( QMouseEvent * e);
	void mouseMoveEvent ( QMouseEvent * e);
	/**/


	CLSceneMovement m_movement;
	CLSceneZoom m_scenezoom;
	CLMouseState m_mousestate;

	bool m_handScrolling;
	int m_handMoving;

	VideoWindow* m_selectedWnd;

	QRect m_realSceneRect;
	const VideoCamerasLayout* m_camLayout;
	
};

#endif //PH_graphicsview_h328