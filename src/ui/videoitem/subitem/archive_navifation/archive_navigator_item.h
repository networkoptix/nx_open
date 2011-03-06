#ifndef archive_navigator_item_h1756
#define archive_navigator_item_h1756

#include "..\abstract_sub_item.h"
#include "camera\camera.h"
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

	void setVideoCamera(CLVideoCamera* camera);

	void goToFullScreenMode(bool fullscreen);

protected:
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
	virtual QRectF boundingRect() const;

protected slots:
	virtual void onSubItemPressed(CLAbstractSubItem* subitem);
	void onSliderMoved(int val);

	void sliderPressed();
	void sliderReleased();

	// QT has bug about stile; after set one another one does not work well
	void renewSlider();

protected:
	int m_width;
	int m_height;

	CLImgSubItem* mPlayItem;
	CLImgSubItem* mPauseItem;

	CLImgSubItem* mRewindBackward;
	CLImgSubItem* mRewindForward;

	CLImgSubItem* mStepForward;
	CLImgSubItem* mStepBackward;

	QSlider* mSlider;
	QGraphicsProxyWidget* mSlider_item;
	CLAbstractArchiveReader* mStreamReader;

	bool mPlayMode;
	bool mSliderIsmoving;

	CLVideoCamera* m_videoCamera;
 	CLAbstractArchiveReader* mReader;

	bool mFullScreen;

};

#endif  //archive_navigator_item_h1756