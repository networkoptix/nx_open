#ifndef video_wnd_layout_h1750
#define video_wnd_layout_h1750

#include "device/device.h"
#include "grid_engine.h"

class CLAbstractSceneItem;
class CLVideoWindowItem;
class CLVideoCamera;
class CLDeviceVideoLayout;
class GraphicsView;
class QGraphicsScene;
class CLDevice;
class LayoutContent;
class CLAbstractComplicatedItem;
class CLAbstractSubItemContainer;

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

	void setMaxFps(int max_fps);

	//================================================

	//start should be called only after Layout putted on the scene, but before first item is added
	// lunches timer
	void start();

	// stop must be called before remove layout from the scene, but after animation I  think
	// stops all cams, removes all Items from, the scene
	void stop(bool animation = false);

	//================================================
	bool addLayoutItem(QString name, LayoutContent* lc, bool update_scene_rect);

	bool addDevice(QString uniqueid, bool update_scene_rect);

	//does same as bool addCamera(CLVideoCamera* cam, int x, int y, int z_order = 0);
	//but function peeks up position by it self 
	bool addItem(CLAbstractSceneItem* item,  bool update_scene_rect);

	//================================================

	void setItemDistance(qreal distance);

	void updateSceneRect();

	QList<CLAbstractSceneItem*> getItemList() const;
	QList<CLAbstractSceneItem*>* getItemListPointer();
	bool hasSuchItem(const CLAbstractSceneItem* item) const;

	void makeAllItemsSelectable(bool selectable);

	//========================================================
	void loadContent();
	void saveContent();
	//========================================================

    void removeItems(QList<CLAbstractSubItemContainer*> itemlst);

signals:
	void stoped(LayoutContent* l);
	void onItemPressed(LayoutContent* l, QString itemname);
	void onNewLayoutSelected(LayoutContent* curr, LayoutContent* newl);
	void onNewLayoutItemSelected(LayoutContent* newl);
protected slots:

	void onItemPressed(CLAbstractSceneItem* item);
	void onItemDoubleClick(CLAbstractSceneItem* item);
	void onItemFullScreen(CLAbstractSceneItem* item);
	void onItemSelected(CLAbstractSceneItem* item);

	void onAspectRatioChanged(CLAbstractSceneItem* wnd);
	void onTimer();
	void onVideoTimer();

	void onItemClose(CLAbstractSubItemContainer* item, bool addToremovedLst = true);

	void stop_helper(bool emt = true);
private:

	// position of window( if any ) will be changed
	// return false if there are no available slots
	bool addItem(CLAbstractSceneItem* item, int x, int y, bool update_scene_rect);

	// remove item from lay out
	void removeItem(CLAbstractSceneItem* item, bool update_scene_rect = true);

	// creates video item for device, if needed, camera and so on...
	bool addDevice(CLDevice* dev, bool update_scene_rect);

	// removes devices;
	bool removeDevices( QList<CLAbstractComplicatedItem*> lst); 

	//================================================
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
	bool m_editable;
	bool m_contentchanged;

	CLGridEngine m_grid;
};

//===================================================================
#endif //video_wnd_layout_h1750
