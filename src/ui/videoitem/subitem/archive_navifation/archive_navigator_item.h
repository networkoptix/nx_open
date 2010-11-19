#ifndef archive_navigator_item_h1756
#define archive_navigator_item_h1756

#include "..\abstract_sub_item.h"
class CLImgSubItem;
class QGraphicsProxyWidget;
class QSlider;

class CLArchiveNavigatorItem : public CLAbstractSubItem
{
	Q_OBJECT
public:
	CLArchiveNavigatorItem(CLAbstractSubItemContainer* parent);
	~CLArchiveNavigatorItem();

	// this function uses parent width
	void onResize();

protected:
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
	virtual QRectF boundingRect() const;

protected slots:
	virtual void onSubItemPressed(CLAbstractSubItem* subitem);
	void onSliderValueChanged(int val);

protected:
	int m_width;
	int m_height;

	CLImgSubItem* mPlayItem;
	CLImgSubItem* mPauseItem;
	QSlider* mSlider;
	QGraphicsProxyWidget* mSlider_item;


};

#endif  //archive_navigator_item_h1756