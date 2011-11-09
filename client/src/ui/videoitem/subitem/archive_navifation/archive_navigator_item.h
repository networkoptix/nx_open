#ifndef archive_navigator_item_h1756
#define archive_navigator_item_h1756

#include "../abstract_sub_item.h"
#include "camera/camera.h"
class CLImgSubItem;
class QGraphicsProxyWidget;
class QSlider;
class QnAbstractArchiveReader;

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

    bool isCursorOnSlider() const;

    virtual void hideIfNeeded(int duration);

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

private:
    void resizeSlider();

protected:
    int m_width;
    int m_height;

    bool m_readerIsSet;

    int m_nonFullScreenHeight;

    CLImgSubItem* mPlayItem;
    CLImgSubItem* mPauseItem;

    CLImgSubItem* mRewindBackward;
    CLImgSubItem* mRewindForward;

    CLImgSubItem* mStepForward;
    CLImgSubItem* mStepBackward;

    QSlider* mSlider;
    QGraphicsProxyWidget* mSlider_item;
    QnAbstractArchiveReader* m_streamReader;

    bool m_playMode;
    bool m_sliderIsmoving;

    CLVideoCamera* m_videoCamera;
    QnAbstractArchiveReader* m_reader;

    bool m_fullScreen;

    qreal m_buttonAspectRatio;

};

#endif  //archive_navigator_item_h1756
