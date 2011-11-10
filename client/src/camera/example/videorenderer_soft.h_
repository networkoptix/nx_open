#ifndef QTV_VIDEORENDERER_SOFT_H
#define QTV_VIDEORENDERER_SOFT_H


#include "videowidget.h"
#include "qglobal.h"

class VideoRendererSoftFilter;
//this class is used to render evrything in software (like in the Graphics View)
class VideoRendererSoft : public QObject //this is used to receive events
{
public:
    VideoRendererSoft(QWidget *);
    virtual ~VideoRendererSoft();
    //Implementation from AbstractVideoRenderer
    void repaintCurrentFrame(QWidget *target, const QRect &rect);
	void internalNotifyResize(const QSize &size, const QSize &videoSize);
    void notifyResize(const QRect&);
    QSize videoSize() const;
    void applyMixerSettings(qreal brightness, qreal contrast, qreal hue, qreal saturation);
    bool isNative() const;
	QWidget* target() const
	{
		return m_target;
	}
protected:
    bool event(QEvent *);
private:
	//Filter m_filter;
	int m_dstX, m_dstY, m_dstWidth, m_dstHeight;
    VideoRendererSoftFilter *m_renderer;
    QTransform m_transform;
    QRect m_videoRect; //rectangle where the video is displayed
    QWidget *m_target;
};

#endif
