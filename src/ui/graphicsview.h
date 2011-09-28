#ifndef PH_graphicsview_h328
#define PH_graphicsview_h328

#include <QtGui/QGraphicsView>

#include "animation/scene_movement.h"
#include "animation/scene_zoom.h"
#include "animation/mouse_state.h"
#include "animation/animated_show.h"
#include "video_cam_layout/videocamlayout.h"
#include "ui_common.h"
#include "animation/animation_manager.h"
#include "animation/steady_mouse_animation.h"
#include "mouse_ignore_helper.h"

class NavigationItem;
class CLAbstractSceneItem;
class CLVideoWindowItem;
class QGraphicsPixmapItem;
class CLAnimatedBackGround;
class CLAbstractDeviceSettingsDlg;
class CLAbstractUnmovedItem;
class QParallelAnimationGroup;
class QInputEvent;
class CLSerachEditItem;
class CLGridItem;
class QnPageSelector;

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

    QnPageSelector* getPageSelector() const;

    CLAbstractSceneItem* getSelectedItem() const;

    void setZeroSelection(int delay = 0);

    qreal getZoom() const;

    bool shouldOptimizeDrawing() const;

    void setAllItemsQuality(CLStreamreader::StreamQuality q, bool increase);
    void closeAllDlg();

    void addjustAllStaticItems();
    void centerOn(const QPointF &pos);

    // moves scene in view coord
    void viewMove(int dx, int dy);

    void initDecoration();

    void setViewMode(ViewMode mode);
    ViewMode getViewMode() const;

    void addStaticItem(CLAbstractUnmovedItem* item, bool connect = true);
    void removeStaticItem(CLAbstractUnmovedItem* item);

    void stopAnimation();

    void instantArrange();

    void updateTransform(qreal angle);

    void goToSteadyMode(bool steady);

    // returns true if should stop
    bool onUserInput(bool go_unsteady, bool escapeFromintro);

    void updatePageSelector();

    NavigationItem *getNavigationItem();

signals:
    void scneZoomFinished();
    void onDecorationPressed(LayoutContent*, QString);
    void onIntroScreenEscape();
    void onNewLayoutSelected(LayoutContent*, LayoutContent*);
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

    void onItemFullScreen_helper(CLAbstractSceneItem* wnd, int duration);
    qreal zoomForFullScreen_helper(QRectF rect) const;

    void onArrange_helper();
    void onCircle_helper(bool show=false);

    void show_device_settings_helper(CLDevice* dev);

    CLAbstractSceneItem* getLastSelectedItem();
    void mouseSpeed_helper(qreal& mouse_speed,  int& dx, int&dy, int min_spped, int speed_factor);
    bool isItemStillExists(const CLAbstractSceneItem* wnd) const;

    void drawBackground ( QPainter * painter, const QRectF & rect );

    void showStop_helper();

    //========decorations=====
    void removeAllStaticItems();
    void updateDecorations();
    CLAbstractUnmovedItem* staticItemByName(const QString &name) const;

    //========navigation=======
    void enableMultipleSelection(bool enable, bool unselect = true);
    bool isCTRLPressed(const QInputEvent* event) const;
    bool isALTPressed(const QInputEvent* event) const;


    bool isItemFullScreenZoomed(QGraphicsItem* item) const;
    bool isNavigationMode() const;
    bool mouseIsCloseToNavigationControl(const QPoint &mpos) const;

    CLAbstractSceneItem* navigationItem(QGraphicsItem* item) const;

    //=========================
    void contextMenuHelper_addNewLayout();
    void contextMenuHelper_chngeLayoutTitle(CLAbstractSceneItem* wnd);
    void contextMenuHelper_editLayout(CLAbstractSceneItem* wnd);
    bool contextMenuHelper_existRecordedVideo(CLVideoCamera* cam);
    void contextMenuHelper_viewRecordedVideo(CLVideoCamera* cam);
    void contextMenuHelper_saveRecordedAs(CLVideoCamera* cam);
    void contextMenuHelper_takeScreenshot(CLVideoWindowItem* item);
    void contextMenuHelper_openInWebBroser(CLVideoCamera* cam);
    void contextMenuHelper_Rotation(CLAbstractSceneItem* wnd, qreal angle);
    void contextMenuHelper_restoreLayout();
    void contextMenuHelper_saveLayout( bool new_name);
    //==========================

    void navigation_grid_items_drop_helper();

public slots:

    void fitInView(int duration = 700 , int delay = 0, CLAnimationCurve curve =  SLOW_END_POW_40);
private slots:
    void onSceneZoomFinished();
    void onSecTimer();
    void onDecorationItemPressed(const QString &);
    void onArrange_helper_finished();
    void toggleRecording();
    void recordingSettings();
    void toggleFullScreen();

    void on_grid_drop_animation_finished();
    void onOpenFile();

protected:
    QFileDialog m_openMediaDialog;
    QGraphicsScene m_scene;
    SceneLayout m_camLayout;

    int m_xRotate;
    qreal m_yRotate;

    /**/

    CLSceneMovement m_movement;
    CLSceneZoom m_scenezoom;
    CLMouseState m_mousestate;

    qreal m_min_scene_zoom;

    int m_rotationCounter;

    bool m_handMoving;
    int m_handMovingEventsCounter;
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

    Show_helper m_show;

    CLSteadyMouseAnimation mSteadyShow;

    QTimer m_secTimer;

    bool mZerroDistance;
    qreal m_old_distance;

    bool mViewStarted;
    bool mWheelZooming; // this var is to avoid wheel zooming scene center shifts.

    //====decoration======
    QList<CLAbstractUnmovedItem*> m_staticItems;

    //======fps
    int m_fps_frames;
    QTime m_fps_time;

    ViewMode m_viewMode;

    CLSerachEditItem* m_searchItem;
    QnPageSelector* m_pageSelector;

    CLAnimationManager m_animationManager;

    CLGridItem* m_gridItem;

    bool m_menuIsHere;

    CLMouseIgnoreHelper m_ignoreMouse;

    CLAbstractSceneItem* m_lastPressedItem;

    QPointer<NavigationItem> m_navigationItem;


private:
    QFileInfoList tmpRecordedFiles(CLVideoCamera* cam);
    void removeFileDeviceItem(CLDeviceSearcher& deviceSearcher, CLAbstractSceneItem* aitem);

    friend class SceneLayout;
};


#endif //PH_graphicsview_h328
