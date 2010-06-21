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

	qreal getZoom() const;

	void setAllItemsQuality(CLStreamreader::StreamQuality q, bool increase);

protected:
	virtual void wheelEvent ( QWheelEvent * e );
	void mouseReleaseEvent ( QMouseEvent * e);
	void mousePressEvent ( QMouseEvent * e);
	void mouseMoveEvent ( QMouseEvent * e);
	void mouseDoubleClickEvent ( QMouseEvent * e );

	virtual void keyPressEvent( QKeyEvent * e );

	void resizeEvent ( QResizeEvent * event );

	void onNewItemSelected_helper(VideoWindow* new_wnd);
	void toggleFullScreen_helper(VideoWindow* new_wnd);

	void onItemFullScreen_helper(VideoWindow* wnd);

	VideoWindow* getLastSelectedWnd();

private slots:

protected:
	
	int m_xRotate;
	int m_yRotate;
	
	void updateTransform();

	
	/**/


	CLSceneMovement m_movement;
	CLSceneZoom m_scenezoom;
	CLMouseState m_mousestate;

	bool m_handScrolling;
	int m_handMoving;

	VideoWindow* m_selectedWnd;
	VideoWindow* m_last_selectedWnd;

	QRect m_realSceneRect;
	const VideoCamerasLayout* m_camLayout;

	bool m_ignore_release_event;

	qreal m_fullScreenZoom;
	
};

#endif //PH_graphicsview_h328