#ifndef PH_graphicsview_h328
#define PH_graphicsview_h328

#include <QGraphicsView>
#include "./animation/scene_movement.h"
#include "./animation/scene_zoom.h"
#include "./animation/mouse_state.h"
#include "./animation/animated_show.h"
#include "video_cam_layout/videocamlayout.h"


class CLAbstractSceneItem;
class CLVideoWindowItem;
class MainWnd;
class QGraphicsPixmapItem;
class CLAnimatedBackGround;
class CLAbstractDeviceSettingsDlg;
class CLAbstractUnmovedItem;
class QParallelAnimationGroup;

class GraphicsView: public QGraphicsView 
{
	Q_OBJECT
public:

	enum Decoration{NONE, VIDEO};

	GraphicsView(MainWnd* mainWnd);
	virtual ~GraphicsView();

	void zoomMin(int duration);
	void zoomDefault(int duration);
	void setRealSceneRect(QRect rect);
	QRect getRealSceneRect() const;

	SceneLayout& getCamLayOut();

	CLAbstractSceneItem* getSelectedItem() const;

	void setZeroSelection(); 

	qreal getZoom() const;

	void setAllItemsQuality(CLStreamreader::StreamQuality q, bool increase);
	void closeAllDlg();
	void stopAnimation();
	
	void setAcceptInput(bool accept); 


	void addjustAllStaticItems();
	void setDecoration(Decoration dec);
	void centerOn(const QPointF &pos);

signals:
	void scneZoomFinished();
	void onPressed(QString, QString);
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

	//=========================
	void onNewItemSelected_helper(CLAbstractSceneItem* new_wnd, int delay);
	void toggleFullScreen_helper(CLAbstractSceneItem* new_wnd);

	void onItemFullScreen_helper(CLAbstractSceneItem* wnd);
	
	void onArrange_helper();
	void onCircle_helper(bool show=false);

	void show_device_settings_helper(CLDevice* dev);
	
	CLAbstractSceneItem* getLastSelectedItem();
	void mouseSpeed_helper(qreal& mouse_speed,  int& dx, int&dy, int min_spped, int speed_factor);
	bool isItemStillExists(const CLAbstractSceneItem* wnd) const;

	void drawBackground ( QPainter * painter, const QRectF & rect );

	void showStop_helper();
	void stopGroupAnimation();


	//========decorations=====
	void addStaticItem(CLAbstractUnmovedItem* item);
	void removeAllStaticItems();
	


public slots:
	
	void fitInView(int duration = 600);
private slots:
	void onScneZoomFinished();
	void reAdjustSceneRect();
	void onShowTimer();
	void onShowStart();
	void onStaticItemPressed(QString);
protected:
	
	QGraphicsScene m_scene;
	SceneLayout m_camLayout;

	int m_xRotate;
	qreal m_yRotate;
	
	void updateTransform(qreal angle);

	
	/**/


	CLSceneMovement m_movement;
	CLSceneZoom m_scenezoom;
	CLMouseState m_mousestate;

	
	int m_rotationCounter;

	bool m_handScrolling;
	int m_handMoving;



	CLAbstractSceneItem* m_selectedWnd;
	CLAbstractSceneItem* m_last_selectedWnd;
	CLAbstractSceneItem* m_rotatingWnd;
	CLAbstractSceneItem* m_movingWnd;


	QRect m_realSceneRect;
	


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

	bool mAcceptInput;
	
	//====decoration======
	QList<CLAbstractUnmovedItem*> m_staticItems;

	QParallelAnimationGroup *m_groupAnimation;

};

#endif //PH_graphicsview_h328