#ifndef PH_graphicsview_h328
#define PH_graphicsview_h328

#include <QGraphicsView>
#include "./videodisplay/animation/scene_movement.h"
#include "./videodisplay/animation/scene_zoom.h"
#include "./videodisplay/animation/mouse_state.h"
#include "./videodisplay/menu/grapicsview_context_menu.h"

class VideoWindow;
class VideoCamerasLayout;
class MainWnd;

class GraphicsView: public QGraphicsView , public QViewMenuHandler
{
	Q_OBJECT
public:
	GraphicsView(MainWnd* mainWnd);
	virtual ~GraphicsView();

	// this function must be called after view gets it's scene
	void init();

	void zoomDefault(int duration);
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
	void contextMenuEvent ( QContextMenuEvent * event );

	virtual void keyPressEvent( QKeyEvent * e );
	virtual void keyReleaseEvent( QKeyEvent * e );

	void resizeEvent ( QResizeEvent * event );

	void OnMenuButton(QObject* owner, QString text);

	//=========================
	void onNewItemSelected_helper(VideoWindow* new_wnd);
	void toggleFullScreen_helper(VideoWindow* new_wnd);

	void onItemFullScreen_helper(VideoWindow* wnd);
	
	void onArrange_helper();
	void onShuffle_helper();
	
	VideoWindow* getLastSelectedWnd();
	void mouseSpeed_helper(qreal& mouse_speed,  int& dx, int&dy, int min_spped, int speed_factor);
	bool isWndStillExists(const VideoWindow* wnd) const;
	void befor_scene_zoom_chaged();

	void reAdjustSceneRect();
	

private slots:
	void onFitInView_helper();
protected:
	
	int m_xRotate;
	int m_yRotate;
	
	void updateTransform();

	
	/**/


	CLSceneMovement m_movement;
	CLSceneZoom m_scenezoom;
	CLMouseState m_mousestate;

	
	int m_rotationCounter;

	bool m_handScrolling;
	int m_handMoving;



	VideoWindow* m_selectedWnd;
	VideoWindow* m_last_selectedWnd;
	VideoWindow* m_rotatingWnd;
	VideoWindow* m_movingWnd;

	QRect m_realSceneRect;
	const VideoCamerasLayout* m_camLayout;

	bool m_ignore_release_event, m_ignore_conext_menu_event;

	qreal m_fullScreenZoom;

	bool m_CTRL_pressed;

	QViewMenu mVoidMenu;
	MainWnd* mMainWnd;

	
};

#endif //PH_graphicsview_h328