#ifndef video_wnd_layout_h1750
#define video_wnd_layout_h1750

#include <QList>
#include <QRect>
#include <QObject>
#include <QTimer>
#include "layout_content.h"


class CLAbstractSceneItem;
class CLVideoWindowItem;
class CLVideoCamera;
class CLDeviceVideoLayout;
class GraphicsView;
class QGraphicsScene;
class CLDevice;

struct CLIdealWndPos
{
	CLVideoWindowItem* item;
	QPoint pos;
};


class SceneLayout : public QObject
{
	Q_OBJECT
public:
	SceneLayout(); // item distance is preferable distance between windows in % of wnd size
	~SceneLayout();

	//================================================
	void setView(GraphicsView* view);
	void setScene(QGraphicsScene* scene);
	void setEventHandler(QObject* eventhandler);
	void setName(const QString& name);
	QString getName() const;
	void setContent(const LayoutContent& cont);

	//================================================

	//start should be called only after Layout putted on the scene, but before first item is added
	// lunches timer
	void start();

	// stop must be called before remove layout from the scene, but after animation I  think
	// stops all cams, removes all Items from, the scene
	void stop();

	//================================================
	// position of window( if any ) will be changed
	// return false if there are no available slots
	bool addItem(CLAbstractSceneItem* item, int x, int y, bool update_scene_rect = true);

	//does same as bool addCamera(CLVideoCamera* cam, int x, int y, int z_order = 0);
	//but function peeks up position by it self 
	bool addItem(CLAbstractSceneItem* item,  bool update_scene_rect = true);

	// remove item from lay out
	void removeItem(CLAbstractSceneItem* item, bool update_scene_rect = true);

	// creates video item for device, if needed, camera and so on...
	bool addDevice(CLDevice* dev);

	// removes device; does the opposite to addDevice
	bool removeDevice(CLDevice* dev); 

	//================================================

	void setItemDistance(qreal distance);
	qreal getItemDistance() const;

	// means that at least one item can be added 
	bool isSpaceAvalable() const;

	QSize getMaxWndSize(const CLDeviceVideoLayout* layout) const;

	void adjustItem(CLAbstractSceneItem* item) const;

	QRect getLayoutRect() const; // scene rect 
	QRect getSmallLayoutRect() const; // scene rect 

	// returns next best available position; returns -1 if not found(all positions are busy);
	// returns false if everything is busy
	bool getNextAvailablePos(QSize size, int &x, int &y) const;
	
	void updateSceneRect();


	QList<CLAbstractSceneItem*> getItemList() const;


	bool hasSuchItem(const CLAbstractSceneItem* item) const;


	// return wnd on the center of the lay out;
	// returns 0 if there is no wnd at all
	CLAbstractSceneItem*  getCenterWnd() const;


	CLAbstractSceneItem* getNextLeftWnd(const CLAbstractSceneItem* curr) const;
	CLAbstractSceneItem* getNextRightWnd(const CLAbstractSceneItem* curr) const;
	CLAbstractSceneItem* getNextTopWnd(const CLAbstractSceneItem* curr) const;
	CLAbstractSceneItem* getNextBottomWnd(const CLAbstractSceneItem* curr) const;
	

	QList<CLIdealWndPos> calcArrangedPos() const;

	//========================================================
	void loadContent();
	void saveContent();
	//========================================================
protected slots:
	void onAspectRatioChanged(CLAbstractSceneItem* wnd);
	void onTimer();
	void onVideoTimer();
private:

	

	int next_item_helper_get_quarter(const QPointF& current, const QPointF& other) const;
	CLAbstractSceneItem* next_item_helper(const CLAbstractSceneItem* curr, int dir_c, int dir_f) const;

	QPoint getNextCloserstAvailableForWndSlot_butFrom_list___helper(const CLVideoWindowItem* wnd, QList<CLIdealWndPos>& lst) const;

	// how many slots window occupies 
	int slotsW(CLVideoWindowItem* wnd) const;
	int slotsH(CLVideoWindowItem* wnd) const;
	



	CLAbstractSceneItem* getVeryLeftWnd() const;
	CLAbstractSceneItem* getVeryRightWnd() const;
	CLAbstractSceneItem* getVeryTopWnd() const;
	CLAbstractSceneItem* getVeryBottomWnd() const;

	QPoint getMassCenter() const;

	bool isSlotAvailable(int slot, QSize size) const;
	

	void buildPotantial();
	int bPh_findNext(int *energy, int elemnts);

	int slotFromPos(QPoint p) const;
	QPoint posFromSlot(int slot) const;

	void onFirstSceneAppearance();


private:

	QList<CLAbstractSceneItem*> m_items;

	QList<CLVideoCamera*> m_cams; 


	int m_height;
	int m_width;
	qreal m_item_distance;
	int m_max_items;
	int m_slots;

	int *m_potantial_x;
	int *m_potantial_y;
	int m_total_potential_elemnts;


	GraphicsView* m_view;
	QGraphicsScene* m_scene;
	QObject* m_EventHandler;
	QString m_Name;

	QTimer m_videotimer;
	QTimer m_timer;
	bool m_firstTime;

	bool m_isRunning;

	LayoutContent m_content;
	

};

//===================================================================
#endif //video_wnd_layout_h1750