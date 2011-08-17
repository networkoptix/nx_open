#ifndef clgl_video_window
#define clgl_video_window

#include "core/resource/media_resource_layout.h"
#include "img_item.h"
#include "camera/abstractrenderer.h"

class QPainter;
class QnMediaResourceLayout;
class CLVideoCamera;
class CLGLRenderer;

class CLVideoWindowItem : public CLImageItem, public CLAbstractRenderer
{
    Q_OBJECT

public:
    CLVideoWindowItem(const QnMediaResourceLayout *layout, int max_width, int max_height, const QString &name = QString());
    virtual ~CLVideoWindowItem();

    void draw(CLVideoDecoderOutput& image, unsigned int channel);

//    CLVideoCamera* getVideoCam() const;
    float aspectRatio() const;

    virtual QSize sizeOnScreen(unsigned int channel) const;

    virtual bool constantDownscaleFactor() const;

    virtual void beforeDestroy();

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);

    inline const QnMediaResourceLayout* getVideoLayout() const
    { return m_videolayout; }

    virtual bool isZoomable() const;

    void copyVideoDataBeforePainting(bool copy);

    virtual void setItemSelected(bool sel, bool animate = true, int delay = 0);

    virtual void goToSteadyMode(bool steady, bool instant);

signals:
    void onAspectRatioChanged(CLAbstractSceneItem* item);

protected:
    QPointF getBestSubItemPos(CLSubItemType type);

    void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);

    virtual bool wantText() const;

    virtual void drawLostConnection(QPainter* painter);
    virtual void drawGLfailaure(QPainter* painter);

    void saveGLState();
    void restoreGLState();

    QRect getSubChannelRect(unsigned int channel) const;

protected:
    CLGLRenderer *m_gldraw[CL_MAX_CHANNELS];
    bool m_first_draw;

    QFont m_FPS_Font;

    int m_opacity;

    const QnMediaResourceLayout *m_videolayout;
    unsigned int m_videonum;
};

#endif //clgl_draw_h_20_31
