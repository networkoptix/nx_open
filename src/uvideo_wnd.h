#ifndef uvideo_wnd_h_1951
#define uvideo_wnd_h_1951

#include "videodisplay/video_window.h"
#include "./videodisplay/animation/item_zoom.h"

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

private:
	bool m_selected;
	int m_z;

	CLItemZoom m_zoom;

	
};


#endif //uvideo_wnd_h_1951