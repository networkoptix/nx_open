#ifndef video_wnd_layout_h1750
#define video_wnd_layout_h1750


#include "grid_engine.h"
#include "core/resource/resource.h"
#include "camera/abstractrenderer.h"

class CLAbstractSceneItem;
class CLVideoWindowItem;
class CLVideoCamera;
class QnVideoResourceLayout;
class GraphicsView;
class QGraphicsScene;
class QnResource;
class LayoutContent;
class CLAbstractComplicatedItem;
class CLAbstractSubItemContainer;
class QnArchiveSyncPlayWrapper;
class QnRenderWatchMixin;

class SceneLayout : public QObject
{
	Q_OBJECT
public:
	SceneLayout(); // item distance is preferable distance between windows in % of wnd size
	~SceneLayout();

	//================================================
	void setView(GraphicsView* view);
	void setScene(QGraphicsScene* scene);
	void setContent(LayoutContent* cont);
	LayoutContent* getContent();
	CLGridEngine& getGridEngine();

	bool isEditable() const;
	void setEditable(bool editable) ;

	bool isContentChanged() const;
	void setContentChanged(bool changed);

    bool hasLiveCameras() const;

	//================================================

	//start should be called only after Layout putted on the scene, but before first item is added
	// lunches timer
	void start();

	// stop must be called before remove layout from the scene, but after animation I  think
	// stops all cams, removes all Items from, the scene
	void stop(bool animation = false);

	//================================================
	bool addLayoutItem(QString name, LayoutContent* lc, bool update_scene_rect, CLBasicLayoutItemSettings itemSettings = CLBasicLayoutItemSettings());

	bool addDevice(QString uniqueid, bool update_scene_rect, CLBasicLayoutItemSettings itemSettings = CLBasicLayoutItemSettings());


    // if itemSettings non default => it will be used,
    // else position of window( if any ) will be changed
    // return false if there are no available slots
    bool addItem(CLAbstractSceneItem* item, bool update_scene_rect, CLBasicLayoutItemSettings itemSettings = CLBasicLayoutItemSettings());


	//================================================

	void setItemDistance(qreal distance);

	void updateSceneRect();

	QList<CLAbstractSceneItem*> getItemList() const;
	QList<CLAbstractSceneItem*>* getItemListPointer();
	bool hasSuchItem(const CLAbstractSceneItem* item) const;

	void makeAllItemsSelectable(bool selectable);

	//========================================================
	void loadContent();
    void saveLayoutContent();
	//========================================================

    void removeItems(QList<CLAbstractSubItemContainer*> itemlst, bool removeFromLayoutcontent);


    QnRenderWatchMixin *renderWatcher() const {
        return m_renderWatcher;
    }

signals:
	void stoped(LayoutContent* l);
	void onItemPressed(LayoutContent* l, QString itemname);
	void onNewLayoutSelected(LayoutContent* curr, LayoutContent* newl);
	void onNewLayoutItemSelected(LayoutContent* newl);
    void reachedTheEnd();
    void consumerBlocksReader(QnAbstractStreamDataProvider* dataProvider, bool value);
protected slots:

    void onNewPageSelected(int page); 

	void onItemPressed(CLAbstractSceneItem* item);
	void onItemDoubleClick(CLAbstractSceneItem* item);
	void onItemFullScreen(CLAbstractSceneItem* item);
	void onItemSelected(CLAbstractSceneItem* item);
    void onNeedToUpdateItem(CLAbstractSceneItem* );
    void onReachedTheEnd();

	void onAspectRatioChanged(CLAbstractSceneItem* wnd);
	void onTimer();
	void onVideoTimer();

	void onItemClose(CLAbstractSubItemContainer* item, bool addToremovedLst = true, bool removeFromLayoutcontent = true);
	void onItemMakeScreenshot(CLAbstractSubItemContainer* item);

	void stop_helper(bool emt = true);
    void onDisplayingStateChanged(CLAbstractRenderer* renderer, bool value);
private:
	// remove item from lay out
	void removeItem(CLAbstractSceneItem* item, bool update_scene_rect = true);

	// creates video item for device, if needed, camera and so on...
	bool addDevice(QnResourcePtr dev, bool update_scene_rect, CLBasicLayoutItemSettings itemSettings = CLBasicLayoutItemSettings());

	// removes devices;
	bool removeDevices(QList<CLAbstractComplicatedItem*> lst, bool removeFromLayoutcontent); 

    void disconnectFromSyncPlay(CLAbstractComplicatedItem* devitem);
	//================================================


    void addDeviceNavigation(CLAbstractSceneItem* item);
    void removeDeviceNavigation(CLAbstractSceneItem* item);

private:

	QList<CLAbstractSceneItem*> m_items;

	QList<CLAbstractComplicatedItem*> m_deviceitems; 

    QList<QString> m_deletedIds; // list of deleted items from scene (search will not show such item again unless serach line is empty )

	GraphicsView* m_view;
	QGraphicsScene* m_scene;

	QTimer m_videotimer;
	QTimer m_timer;
	bool m_firstTime;

	bool m_isRunning;

	LayoutContent* m_content;
    LayoutContent* m_contentCopy;

	bool m_editable;
	bool m_contentchanged;

    QString mSearchFilter;

	CLGridEngine m_grid;

    QnArchiveSyncPlayWrapper* m_syncPlay;

    QnRenderWatchMixin *m_renderWatcher;
};

//===================================================================
#endif //video_wnd_layout_h1750
