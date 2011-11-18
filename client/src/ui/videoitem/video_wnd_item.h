#ifndef clgl_video_window
#define clgl_video_window

#include "img_item.h"
#include "camera/abstractrenderer.h"
#include "camera/gl_renderer.h"
#include "core/dataprovider/streamdata_statistics.h"
#include "core/resource/resource_media_layout.h"

class QPainter;
class MainWnd;
class CLAbstractVideoHolder;
class QnVideoResourceLayout;
class NavigationItem;
class CLVideoCamera;
struct CLVideoDecoderOutput;

class CLVideoWindowItem : public CLImageItem, public CLAbstractRenderer
{
    Q_OBJECT
public:
    CLVideoWindowItem(GraphicsView *view, const QnVideoResourceLayout *layout, int max_width, int max_height, QString name = QString());
    virtual ~CLVideoWindowItem();

    virtual void draw(CLVideoDecoderOutput* image) override;
    virtual void waitForFrameDisplayed(int channel) override;


    CLVideoCamera* getVideoCam() const;
    float aspectRatio() const;

    virtual QSize sizeOnScreen(unsigned int channel) const;

    virtual bool constantDownscaleFactor() const;

    virtual void beforeDestroy();

    void showFPS(bool show);
    void setStatistics(QnStatistics * st, unsigned int channel);

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);

    const QnVideoResourceLayout* getVideoLayout() const
    {
        return m_videolayout;
    }

    virtual bool isZoomable() const;

    virtual void setItemSelected(bool sel, bool animate = true, int delay = 0);

    virtual void goToSteadyMode(bool steady, bool instant);
    QImage getScreenshot();
    bool contains(CLAbstractRenderer* renderer) const;
protected:

    QPointF getBestSubItemPos(CLSubItemType type);

    void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);

    virtual bool wantText() const;

    virtual void drawStuff(QPainter* painter);

    virtual void drawFPS(QPainter* painter);
    virtual void drawLostConnection(QPainter* painter);
    virtual void drawGLfailaure(QPainter* painter);

    void saveGLState();
    void restoreGLState();

    QRect getSubChannelRect(unsigned int channel) const;

protected:
    CLGLRenderer* m_gldraw[CL_MAX_CHANNELS];
    bool m_first_draw;
    QnStatistics* m_stat[CL_MAX_CHANNELS];

    bool m_showfps;

    QFont m_FPS_Font;

    int m_opacity;

    const QnVideoResourceLayout* m_videolayout;
    unsigned int m_videonum;
};

#endif //clgl_draw_h_20_31
