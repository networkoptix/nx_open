#ifndef PH_graphicsview_h328
#define PH_graphicsview_h328

#include <QGraphicsView>
#include "./animation/scene_movement.h"
#include "./animation/scene_zoom.h"
#include "./animation/mouse_state.h"
#include "./animation/animated_show.h"
#include "video_cam_layout/videocamlayout.h"
#include "ui_common.h"


class CLAbstractSceneItem;
class CLVideoWindowItem;
class QGraphicsPixmapItem;
class CLAnimatedBackGround;
class CLAbstractDeviceSettingsDlg;
class CLAbstractUnmovedItem;
class QParallelAnimationGroup;
class QInputEvent;
class CLSerachEditItem;

class GraphicsView: public QGraphicsView 
{
	Q_OBJECT
public:
	enum ViewMode{NormalView, ItemsDonor, ItemsAcceptor};

	GraphicsView(QWidget* mainWnd);
	virtual ~GraphicsView();

	void start();
	void stop();

	void zoomMin(int duration);
	void setRealSceneRect(QRect rect);
	QRect getRealSceneRect() const;

	qreal getMinSceneZoom() const;
	void setMinSceneZoom(qreal z);

	SceneLayout& getCamLayOut();

	CLAbstractSceneItem* getSelectedItem() const;

	void setZeroSelection(); 

	qreal getZoom() const;

	void setAllItemsQuality(CLStreamreader::StreamQuality q, bool increase);
	void closeAllDlg();
		

	void addjustAllStaticItems();
	void centerOn(const QPointF &pos);

	void initDecoration();

	void setViewMode(ViewMode mode);
	ViewMode getViewMode() const;


	void addStaticItem(CLAbstractUnmovedItem* item, bool connect = true);
	void removeStaticItem(CLAbstractUnmovedItem* item);

	void stopAnimation();


signals:
	void scneZoomFinished();
	void onDecorationPressed(LayoutContent*, QString);
protected:
	virtual void wheelEvent ( QWheelEvent * e );
	void mouseReleaseEvent ( QMouseEvent * e);
	void mousePressEvent ( QMouseEvent * e);
	void mouseMoveEvent ( QMouseEvent * e);
	void mouseDoubleClickEvent ( QMouseEvent * e );
	void contextMenuEvent ( QContextMenuEvent * event );

	void dragEnterEvent ( QDragEnterEvent * event );
	void dragMoveEvent ( QDragMoveEvent * event );
	void dropEvent ( QDropEvent * event );
	

	virtual void keyPressEvent( QKeyEvent * e );
	virtual void keyReleaseEvent( QKeyEvent * e );

	
	void resizeEvent(QResizeEvent * event);

	//=========================
	void onNewItemSelected_helper(CLAbstractSceneItem* new_wnd, int delay);
	void toggleFullScreen_helper(CLAbstractSceneItem* new_wnd);

	void onItemFullScreen_helper(CLAbstractSceneItem* wnd);
	qreal zoomForFullScreen_helper(QRectF rect) const;
	
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
	void removeAllStaticItems();
	void updateDecorations();
	CLAbstractUnmovedItem* staticItemByName(QString name) const;
	void recalcSomeParams();
	
	
	//========navigation=======
	void enableMultipleSelection(bool enable, bool unselect = true); 
	bool isCTRLPressed(const QInputEvent* event) const;

	bool isItemFullScreenZoomed(QGraphicsItem* item);

	CLAbstractSceneItem* navigationItem(QGraphicsItem* item) const;

	//=========================
	void contextMenuHelper_addNewLayout();
	void contextMenuHelper_chngeLayoutTitle(CLAbstractSceneItem* wnd);
	void contextMenuHelper_editLayout(CLAbstractSceneItem* wnd);
	void contextMenuHelper_viewRecordedVideo(CLVideoCamera* cam);
	void contextMenuHelper_openInWebBroser(CLVideoCamera* cam);
	void contextMenuHelper_Rotation(CLAbstractSceneItem* wnd, qreal angle);

public slots:
	
	void fitInView(int duration = 600 , int delay = 0, CLAnimationTimeLine::CLAnimationCurve curve =  CLAnimationTimeLine::SLOW_END_POW_40 );
private slots:
	void onScneZoomFinished();
	void onShowTimer();
	void onShowStart();
	void onDecorationItemPressed(QString);
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

	qreal m_min_scene_zoom;
	
	int m_rotationCounter;

	bool m_handScrolling;
	int m_handMoving;
	bool mSubItemMoving;



	CLAbstractSceneItem* m_selectedWnd;
	CLAbstractSceneItem* m_last_selectedWnd;
	CLAbstractSceneItem* m_rotatingWnd;
	unsigned int m_movingWnd;


	QRect m_realSceneRect;
	


	bool m_ignore_release_event, m_ignore_conext_menu_event;

	QWidget* mMainWnd;

	CLAbstractDeviceSettingsDlg* mDeviceDlg;

	bool m_drawBkg;
	CLAnimatedBackGround* m_animated_bckg;
	QGraphicsPixmapItem* m_logo;

	Show_helper mShow;

	bool mZerroDistance;
	qreal m_old_distance;

	bool mViewStarted;
	
	//====decoration======
	QList<CLAbstractUnmovedItem*> m_staticItems;
	QParallelAnimationGroup *m_groupAnimation;

	//======fps
	int m_fps_frames;
	QTime m_fps_time;


	ViewMode m_viewMode;

	CLSerachEditItem* m_seachItem;


};

#endif //PH_graphicsview_h328