#ifndef uvideo_wnd_h_1951
#define uvideo_wnd_h_1951

#include "videodisplay/video_window.h"
#include "./videodisplay/animation/animation_timeline.h"

class VideoWindow :  public CLVideoWindow
{
	Q_OBJECT
public:
	VideoWindow(const CLDeviceVideoLayout* layout, int max_width, int max_height);



	void setSelected(bool sel);


protected:
	virtual void drawStuff(QPainter* painter);

	void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
	void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);

private:

	void drawSelection(QPainter* painter);

private slots:
	void setMouseMoveZoom(int zoom);
	void updateZvalue();


private:
	bool m_selected;
	int m_z;

	CLAnimationTimeLine m_zoomTimeLine;
};


#endif //uvideo_wnd_h_1951