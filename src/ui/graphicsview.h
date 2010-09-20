#ifndef PH_graphicsview_h328
#define PH_graphicsview_h328

#include <QGraphicsView>
#include "./animation/scene_movement.h"
#include "./animation/scene_zoom.h"
#include "./animation/mouse_state.h"
#include "./animation/animated_show.h"


class VideoWindow;
class VideoCamerasLayout;
class MainWnd;
class QGraphicsPixmapItem;
class CLAnimatedBackGround;
class CLAbstractDeviceSettingsDlg;

class GraphicsView: public QGraphicsView 
{
	Q_OBJECT
public:
	GraphicsView(MainWnd* mainWnd);
	virtual ~GraphicsView();

	void zoomDefault(int duration);
	void setRealSceneRect(QRect rect);
	QRect getRealSceneRect() const;

	void setCamLayOut(VideoCamerasLayout *);

	VideoWindow* getSelectedWnd() const;

	void setZeroSelection(); 

	qreal getZoom() const;

	void setAllItemsQuality(CLStreamreader::StreamQuality q, bool increase);

	void closeAllDlg();

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

	void OnMenuButton(void* owner, QString text);

	//=========================
	void onNewItemSelected_helper(VideoWindow* new_wnd, int delay);
	void toggleFullScreen_helper(VideoWindow* new_wnd);

	void onItemFullScreen_helper(VideoWindow* wnd);
	
	void onArrange_helper();
	void onCircle_helper(bool show=false);

	void show_device_settings_helper(CLDevice* dev);
	
	VideoWindow* getLastSelectedWnd();
	void mouseSpeed_helper(qreal& mouse_speed,  int& dx, int&dy, int min_spped, int speed_factor);
	bool isWndStillExists(const VideoWindow* wnd) const;


	

	void drawBackground ( QPainter * painter, const QRectF & rect );


private slots:
	void onFitInView_helper(int duration = 600);
	void reAdjustSceneRect();
	void onShowTimer();
	void onShowStart();
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
	VideoCamerasLayout* m_camLayout;


	bool m_ignore_release_event, m_ignore_conext_menu_event;

	qreal m_fullScreenZoom;

	bool m_CTRL_pressed;

	MainWnd* mMainWnd;

	CLAbstractDeviceSettingsDlg* mDeviceDlg;

	bool m_drawBkg;
	CLAnimatedBackGround* m_animated_bckg;
	QGraphicsPixmapItem* m_logo;

	Show_helper mShow;

	bool mZerroDistance;
	qreal m_old_distance;
	


};

#endif //PH_graphicsview_h328