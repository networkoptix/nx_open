#ifndef clgl_video_window
#define clgl_video_window

#include "abstractrenderer.h"
#include <QtOpenGL/QGLWidget>
#include <QMutex>
#include <QWaitCondition>
#include "gl_renderer.h"
#include "../statistics/statistics.h"
#include <QGraphicsItem>
#include "../device/device_video_layout.h"

class QPainter;
class MainWnd;
class CLAbstractVideoHolder;
class CLDeviceVideoLayout;
class CLVideoCamera;


class CLVideoWindow : public QObject, public QGraphicsItem, public CLAbstractRenderer
{
	Q_OBJECT
public:


	CLVideoWindow (const CLDeviceVideoLayout* layout, int max_width, int max_height);
	virtual ~CLVideoWindow ();
	void draw(CLVideoDecoderOutput& image, unsigned int channel);

	void setVideoCam(CLVideoCamera* cam);
	CLVideoCamera* getVideoCam() const;

	virtual void before_destroy();
	void applyMixerSettings(qreal brightness, qreal contrast, qreal hue, qreal saturation);
	virtual float aspectRatio() const;

	void showFPS(bool show);
	void setStatistics(CLStatistics * st, unsigned int channel);

	void setInfoText(QString text);


	QRectF boundingRect() const;
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);

	int height() const;
	int width() const;

	int imageWidth() const;
	int imageHeight() const;

	void setArranged(bool arr);
	bool isArranged() const;

	void setShowFps(bool show);
	void setShowInfoText(bool show);
	void setShowImagesize(bool show);

	void setOpacity(int opacity);

	const CLDeviceVideoLayout* getVideoLayout() const 
	{
		return m_videolayout;
	}


	bool needUpdate() const
	{
		return m_needUpdate;
	}

	void needUpdate(bool val)
	{
		m_needUpdate = val;
	}

	void copyVideoDataBeforePainting(bool copy);

signals:

	void onVideoItemSelected(CLVideoWindow*);
	void onVideoItemMouseRightClick(CLVideoWindow*);


protected:
	
	virtual void drawStuff(QPainter* painter);

	virtual void drawFPS(QPainter* painter);
	virtual void drawLostConnection(QPainter* painter);
	virtual void drawInfoText(QPainter* painter);

	virtual void onAspectRatioChanged()=0;



	void saveGLState();
	void restoreGLState();

	void focusInEvent ( QFocusEvent * event );
	void mousePressEvent ( QGraphicsSceneMouseEvent * event );

	QRect getSubChannelRect(unsigned int channel) const;
protected:
	CLGLRenderer* m_gldraw[CL_MAX_CHANNELS];
	bool m_first_draw;
	CLStatistics* m_stat[CL_MAX_CHANNELS];

	bool m_showfps;
	bool m_showinfotext;
	bool m_showimagesize;

	QFont m_FPS_Font;
	QFont m_Info_Font;
	
	int m_max_width, m_max_heght;

	QString m_infoText;

	mutable QMutex m_mutex;
	int m_imageWidth;
	int m_imageHeight;

	int m_imageWidth_old;
	int m_imageHeight_old;

	bool m_arranged;



	bool m_needUpdate;

	int m_opacity;

	const CLDeviceVideoLayout* m_videolayout;
	unsigned int m_videonum;
	CLVideoCamera* m_cam;
	
	
};


#endif //clgl_draw_h_20_31