#ifndef video_wnd_layout_h1750
#define video_wnd_layout_h1750

#include <QList>
#include <QRect>
#include <QObject>


class CLAbstractSceneItem;
class CLVideoWindowItem;
class CLVideoCamera;
class CLDeviceVideoLayout;
class GraphicsView;
class QGraphicsScene;

struct CLIdealWndPos
{
	CLVideoWindowItem* item;
	QPoint pos;
};


// class provides just helper tool to navigate layouts 
// takes in account that one cam can have few items
class SceneLayout : public QObject
{
	Q_OBJECT
public:
	SceneLayout(GraphicsView* view, QGraphicsScene* scene, unsigned int max_rows, int item_distance); // item distance is preferable distance between windows in % of wnd size
	~SceneLayout();

	void setItemDistance(qreal distance);
	qreal getItemDistance() const;

	// means that at least one item can be added 
	bool isSpaceAvalable() const;

	QSize getMaxWndSize(const CLDeviceVideoLayout* layout) const;

	void adjustWnd(CLVideoWindowItem* wnd) const;

	QRect getLayoutRect() const; // scene rect 
	QRect getSmallLayoutRect() const; // scene rect 

	// returns next best available position; returns -1 if not found(all positions are busy);
	// returns false if everything is busy
	bool getNextAvailablePos(const CLDeviceVideoLayout* layout, int &x, int &y) const;

	// position of window( if any ) will be changed
	// return false if there are no available slots
	bool addWnd(CLVideoWindowItem* wnd, int x, int y, int z_order = 0, bool update_scene_rect = true);

	//does same as bool addCamera(CLVideoCamera* cam, int x, int y, int z_order = 0);
	//but function peeks up position by it self 
	bool addWnd(CLVideoWindowItem* wnd,  int z_order = 0, bool update_scene_rect = true);

		
	// remove item from lay out
	void removeWnd(CLAbstractSceneItem* wnd, bool update_scene_rect = true);

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

protected slots:
	void onAspectRatioChanged(CLVideoWindowItem* wnd);
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
	bool isSlotAvailable(int slot, const CLDeviceVideoLayout* layout) const;

	void buildPotantial();
	int bPh_findNext(int *energy, int elemnts);

	int slotFromPos(QPoint p) const;
	QPoint posFromSlot(int slot) const;


private:

	QList<CLAbstractSceneItem*> m_items;
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

};

//===================================================================
#endif //video_wnd_layout_h1750