#ifndef video_wnd_layout_h1750
#define video_wnd_layout_h1750

#include <QList>
#include <QSet>
#include <QRect>


class CLVideoWindowItem;
class CLVideoCamera;
class CLDeviceVideoLayout;
class GraphicsView;
class QGraphicsScene;

struct CLIdealWndPos
{
	CLVideoWindowItem* wnd;
	QPoint pos;
};


// class provides just helper tool to navigate layouts 
// takes in account that one cam can have few wnd
class VideoCamerasLayout : public QObject
{
	Q_OBJECT
public:
	VideoCamerasLayout(GraphicsView* view, QGraphicsScene* scene, unsigned int max_rows, int item_distance); // item distance is preferable distance between windows in % of wnd size
	~VideoCamerasLayout();

	void setItemDistance(qreal distance);
	qreal getItemDistance() const;

	// means that at least one wnd can be added 
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

		
	// remove wnd from lay out
	void removeWnd(CLVideoWindowItem* wnd, bool update_scene_rect = true);

	void updateSceneRect();


	QSet<CLVideoWindowItem*> getWndList() const;


	bool hasSuchWnd(const CLVideoWindowItem*) const;
	bool hasSuchCam(const CLVideoCamera*) const;


	// return wnd on the center of the lay out;
	// returns 0 if there is no wnd at all
	CLVideoWindowItem*  getCenterWnd() const;


	CLVideoWindowItem* getNextLeftWnd(const CLVideoWindowItem* curr) const;
	CLVideoWindowItem* getNextRightWnd(const CLVideoWindowItem* curr) const;
	CLVideoWindowItem* getNextTopWnd(const CLVideoWindowItem* curr) const;
	CLVideoWindowItem* getNextBottomWnd(const CLVideoWindowItem* curr) const;
	

	QList<CLIdealWndPos> calcArrangedPos() const;

protected slots:
	void onAspectRatioChanged(CLVideoWindowItem* wnd);
private:

	

	int next_wnd_helper_get_quarter(const QPointF& current, const QPointF& other) const;
	CLVideoWindowItem* next_wnd_helper(const CLVideoWindowItem* curr, int dir_c, int dir_f) const;

	QPoint getNextCloserstAvailableForWndSlot_butFrom_list___helper(const CLVideoWindowItem* wnd, QList<CLIdealWndPos>& lst) const;

	// how many slots window occupies 
	int slotsW(CLVideoWindowItem* wnd) const;
	int slotsH(CLVideoWindowItem* wnd) const;
	



	CLVideoWindowItem* getVeryLeftWnd() const;
	CLVideoWindowItem* getVeryRightWnd() const;
	CLVideoWindowItem* getVeryTopWnd() const;
	CLVideoWindowItem* getVeryBottomWnd() const;

	QPoint getMassCenter() const;

	bool isSlotAvailable(int slot, QSize size) const;
	bool isSlotAvailable(int slot, const CLDeviceVideoLayout* layout) const;

	void buildPotantial();
	int bPh_findNext(int *energy, int elemnts);

	int slotFromPos(QPoint p) const;
	QPoint posFromSlot(int slot) const;


private:

	QSet<CLVideoWindowItem*> m_wnds;
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