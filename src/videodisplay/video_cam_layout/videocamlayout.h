#ifndef video_wnd_layout_h1750
#define video_wnd_layout_h1750

#include <QList>
#include <QSet>
#include <QRect>


class CLVideoWindow;
class CLVideoCamera;
class CLDeviceVideoLayout;
class GraphicsView;
class QGraphicsScene;

struct CLIdealWndPos
{
	CLVideoWindow* wnd;
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

	// means that at least one wnd can be added 
	bool isSpaceAvalable() const;

	QSize getMaxWndSize(const CLDeviceVideoLayout* layout) const;

	void adjustWnd(CLVideoWindow* wnd) const;

	QRect getLayoutRect() const; // scene rect 
	QRect getSmallLayoutRect() const; // scene rect 

	// returns next best available position; returns -1 if not found(all positions are busy);
	// returns false if everything is busy
	bool getNextAvailablePos(const CLDeviceVideoLayout* layout, int &x, int &y) const;

	// position of window( if any ) will be changed
	// return false if there are no available slots
	bool addWnd(CLVideoWindow* wnd, int x, int y, int z_order = 0, bool update_scene_rect = true);

	//does same as bool addCamera(CLVideoCamera* cam, int x, int y, int z_order = 0);
	//but function peeks up position by it self 
	bool addWnd(CLVideoWindow* wnd,  int z_order = 0, bool update_scene_rect = true);

		
	// remove wnd from lay out
	void removeWnd(CLVideoWindow* wnd, bool update_scene_rect = true);

	void updateSceneRect();


	QSet<CLVideoWindow*> getWndList() const;


	bool hasSuchWnd(const CLVideoWindow*) const;
	bool hasSuchCam(const CLVideoCamera*) const;


	// return wnd on the center of the lay out;
	// returns 0 if there is no wnd at all
	CLVideoWindow*  getCenterWnd() const;


	CLVideoWindow* getNextLeftWnd(const CLVideoWindow* curr) const;
	CLVideoWindow* getNextRightWnd(const CLVideoWindow* curr) const;
	CLVideoWindow* getNextTopWnd(const CLVideoWindow* curr) const;
	CLVideoWindow* getNextBottomWnd(const CLVideoWindow* curr) const;
	

	QList<CLIdealWndPos> calcArrangedPos() const;

protected slots:
	void onAspectRatioChanged(CLVideoWindow* wnd);
private:

	

	int next_wnd_helper_get_quarter(const QPointF& current, const QPointF& other) const;
	CLVideoWindow* next_wnd_helper(const CLVideoWindow* curr, int dir_c, int dir_f) const;

	QPoint getNextCloserstAvailableForWndSlot_butFrom_list___helper(const CLVideoWindow* wnd, QList<CLIdealWndPos>& lst) const;

	// how many slots window occupies 
	int slotsW(CLVideoWindow* wnd) const;
	int slotsH(CLVideoWindow* wnd) const;
	



	CLVideoWindow* getVeryLeftWnd() const;
	CLVideoWindow* getVeryRightWnd() const;
	CLVideoWindow* getVeryTopWnd() const;
	CLVideoWindow* getVeryBottomWnd() const;

	QPoint getMassCenter() const;

	bool isSlotAvailable(int slot, QSize size) const;
	bool isSlotAvailable(int slot, const CLDeviceVideoLayout* layout) const;

	void buildPotantial();
	int bPh_findNext(int *energy, int elemnts);

	int slotFromPos(QPoint p) const;
	QPoint posFromSlot(int slot) const;


private:

	QSet<CLVideoWindow*> m_wnds;
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