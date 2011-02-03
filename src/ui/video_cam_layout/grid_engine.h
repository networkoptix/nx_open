#ifndef grid_engine_h1754
#define grid_engine_h1754



class CLAbstractSceneItem;
class CLDeviceVideoLayout;

struct CLIdealItemPos
{
	CLAbstractSceneItem* item;
	QPoint pos;
};



struct CLGridSettings
{
	int left;
	int top;

	int slot_width;
	int slot_height;

	int max_items;
	qreal item_distance;

	int max_rows;

	qreal optimal_ratio;

	QList<CLAbstractSceneItem*>* items;
};


class CLGridEngine
{
public:
	CLGridEngine();
	virtual ~CLGridEngine();

	void setSettings(const CLGridSettings& sett);

	CLGridSettings& getSettings();

	// returns recommended item size for such grid; ususally caled befor Item is created
	QSize calcDefaultMaxItemSize(const CLDeviceVideoLayout* lo = 0) const;

	// returns actual item size for such item in this grid.;
	QSize getItemMaxSize(const CLAbstractSceneItem* item) const;

	// means that at least one item can be added 
	bool isSpaceAvalable() const;


	// returns grid rect in scene coordinates 
	QRect getGridRect() const;

	QList<CLIdealItemPos> calcArrangedPos() const;

	// returns next best available position; returns -1 if not found(all positions are busy);
	// returns false if everything is busy
	bool getNextAvailablePos(QSize size, int &x, int &y) const;

	void adjustItem(CLAbstractSceneItem* item) const;



	// return wnd on the center of the lay out;
	// returns 0 if there is no wnd at all
	CLAbstractSceneItem*  getCenterWnd() const;


	CLAbstractSceneItem* getNextLeftItem(const CLAbstractSceneItem* curr) const;
	CLAbstractSceneItem* getNextRightItem(const CLAbstractSceneItem* curr) const;
	CLAbstractSceneItem* getNextTopItem(const CLAbstractSceneItem* curr) const;
	CLAbstractSceneItem* getNextBottomItem(const CLAbstractSceneItem* curr) const;

	// returns item on scene if it has same sizes as incoming item.
	// and intersection of the items more than some threshold ( like 60%)
	CLAbstractSceneItem* getItemToSwapWith(CLAbstractSceneItem* item) const;


private:
	CLAbstractSceneItem* next_item_helper(const CLAbstractSceneItem* curr, int dir_c, int dir_f) const;
	int next_item_helper_get_quarter(const QPointF& current, const QPointF& other) const;

	CLAbstractSceneItem* getVeryLeftItem() const;
	CLAbstractSceneItem* getVeryRightItem() const;
	CLAbstractSceneItem* getVeryTopItem() const;
	CLAbstractSceneItem* getVeryBottomItem() const;


	QPoint getMassCenter() const;


	void slotFromPos(QPoint p, int& slot_x, int& slot_y) const;
	QPoint posFromSlot(int slot_x, int slot_y) const;
	bool isSlotAvailable(int slot_x, int slot_y, QSize size) const;
	

	// how many slots window occupies 
	int slotsW(int width) const;
	int slotsH(int height) const;

	//calcArrangedPos helpers
	bool isSlotAvailable_helper(int slot_x, int slot_y, QSize size, QList<CLIdealItemPos>& arranged) const;
	bool getNextAvailablePos_helper(QSize size, int &x, int &y, int current_grid_width, int current_grid_height, QList<CLIdealItemPos>& arranged) const;


private:

	CLGridSettings m_settings;
	
};


#endif //grid_engine_h1754