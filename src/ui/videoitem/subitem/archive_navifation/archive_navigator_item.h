#ifndef archive_navigator_item_h1756
#define archive_navigator_item_h1756

#include "..\abstract_sub_item.h"
class CLImgSubItem;
class QGraphicsProxyWidget;
class QSlider;
class CLAbstractArchiveReader;

class CLArchiveNavigatorItem : public CLAbstractSubItem
{
	Q_OBJECT
public:
	CLArchiveNavigatorItem(CLAbstractSubItemContainer* parent, int height);
	~CLArchiveNavigatorItem();

	// this function uses parent width
	void onResize();

	void updateSliderPos();

protected:
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
	virtual QRectF boundingRect() const;

protected slots:
	virtual void onSubItemPressed(CLAbstractSubItem* subitem);
	void onSliderMoved(int val);
	CLAbstractArchiveReader* reader();

	void sliderPressed();
	void sliderReleased();

protected:
	int m_width;
	int m_height;

	CLImgSubItem* mPlayItem;
	CLImgSubItem* mPauseItem;
	QSlider* mSlider;
	QGraphicsProxyWidget* mSlider_item;
	CLAbstractArchiveReader* mStreamReader;


	bool mPlayMode;
	bool mSliderIsmoving;

};

#endif  //archive_navigator_item_h1756