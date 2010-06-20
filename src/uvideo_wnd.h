#ifndef uvideo_wnd_h_1951
#define uvideo_wnd_h_1951

#include "videodisplay/video_window.h"
#include "./videodisplay/animation/item_trans.h"

class GraphicsView;

class VideoWindow :  public CLVideoWindow
{
	Q_OBJECT
public:
	VideoWindow(GraphicsView* view, const CLDeviceVideoLayout* layout, int max_width, int max_height);



	void setSelected(bool sel, bool animate = true);
	bool isSelected() const;

	void setFullScreen(bool full);
	bool isFullScreen() const;

	void zoom_abs(qreal z, bool instantly);


protected:
	virtual void drawStuff(QPainter* painter);

	void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
	void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);

private:

	void drawSelection(QPainter* painter);

private:
	bool m_selected;
	bool m_fullscreen; // could be only if m_selected

	int m_z;

	CLItemTransform m_animationTransform;
	GraphicsView* m_view;


};


#endif //uvideo_wnd_h_1951