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


class CLVideoWindow : public QObject, public QGraphicsItem, public CLAbstractRenderer
{
	Q_OBJECT
public:

	static bool m_show_smt_butvideo;

	CLVideoWindow (const CLDeviceVideoLayout* layout);
	virtual ~CLVideoWindow ();
	void draw(CLVideoDecoderOutput& image);
	virtual void before_destroy();
	void applyMixerSettings(qreal brightness, qreal contrast, qreal hue, qreal saturation);
	virtual float aspectRatio() const
	{
		return m_gldraw.aspectRatio();
	}

	void showFPS(bool show);
	void setStatistics(CLStatistics * st, unsigned int channel);

	void setInfoText(QString text);


	QRectF boundingRect() const;
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);

	int height() const;
	int width() const;

	int imageWidth() const;
	int imageHeight() const;

	void setShowFps(bool show);
	void setShowInfoText(bool show);
	void setShowImagesize(bool show);

	void setOpacity(int opacity);


	bool needUpdate() const
	{
		return m_needUpdate;
	}

	void needUpdate(bool val)
	{
		m_needUpdate = val;
	}

	virtual void copyVideoDataBeforePainting(bool copy)
	{
		m_gldraw.copyVideoDataBeforePainting(copy);
	}

signals:

	void onVideoItemSelected(CLVideoWindow*);
	void onVideoItemMouseRightClick(CLVideoWindow*);

protected:
	
	virtual void drawStuff(QPainter* painter);

	virtual void drawFPS(QPainter* painter);
	virtual void drawLostConnection(QPainter* painter);
	virtual void drawInfoText(QPainter* painter);


	void saveGLState();
	void restoreGLState();

	void focusInEvent ( QFocusEvent * event );
	void mousePressEvent ( QGraphicsSceneMouseEvent * event );
protected:
	CLGLRenderer m_gldraw;
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


	bool m_needUpdate;

	int m_opacity;

	const CLDeviceVideoLayout* m_videolayout;
	
};


#endif //clgl_draw_h_20_31