#ifndef clgl_video_window
#define clgl_video_window


#include <QtOpenGL/QGLWidget>
#include <QMutex>
#include <QWaitCondition>
#include "../statistics/statistics.h"
#include "../device/device_video_layout.h"
#include "img_item.h"
#include "camera/abstractrenderer.h"
#include "camera/gl_renderer.h"

class QPainter;
class MainWnd;
class CLAbstractVideoHolder;
class CLDeviceVideoLayout;
class CLVideoCamera;


class CLVideoWindowItem : public CLImageItem, public CLAbstractRenderer
{
	Q_OBJECT
public:
	CLVideoWindowItem (GraphicsView* view, const CLDeviceVideoLayout* layout, int max_width, int max_height,
						QString name="");

	virtual ~CLVideoWindowItem ();

	void draw(CLVideoDecoderOutput& image, unsigned int channel);

	virtual void setVideoCam(CLVideoCamera* cam);
	CLVideoCamera* getVideoCam() const;

	float aspectRatio() const;


	virtual void before_destroy();

	void showFPS(bool show);
	void setStatistics(CLStatistics * st, unsigned int channel);

	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);

	const CLDeviceVideoLayout* getVideoLayout() const 
	{
		return m_videolayout;
	}


	void copyVideoDataBeforePainting(bool copy);

	virtual void setItemSelected(bool sel, bool animate = true, int delay = 0);

	
signals:
	void onAspectRatioChanged(CLAbstractSceneItem* item);

protected slots:
	void onResize();

protected:

	QPointF getBestSubItemPos(CLAbstractSubItem::ItemType type);
	
	
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
	CLStatistics* m_stat[CL_MAX_CHANNELS];

	bool m_showfps;


	QFont m_FPS_Font;
	
	int m_opacity;

	const CLDeviceVideoLayout* m_videolayout;
	unsigned int m_videonum;
	CLVideoCamera* m_cam;

	
};


#endif //clgl_draw_h_20_31