#ifndef uvideo_wnd_h_1951
#define uvideo_wnd_h_1951

#include "videodisplay/video_window.h"
#include "./videodisplay/animation/item_trans.h"

class GraphicsView;
class VideoCamerasLayout;

class VideoWindow :  public CLVideoWindow
{
	Q_OBJECT

	Q_PROPERTY(qreal rotation	READ getRotation WRITE setRotation)

public:
	VideoWindow(GraphicsView* view, const CLDeviceVideoLayout* layout, int max_width, int max_height);



	void setSelected(bool sel, bool animate = true);
	bool isSelected() const;

	void zoom_abs(qreal z, int duration);
	void z_rotate_delta(QPointF center, qreal angle, int diration);
	void z_rotate_abs(QPointF center, qreal angle, int duration);

	qreal getRotation() const;
	void setRotation(qreal angle);

	void stop_animation();

	//====rotation=======
	void drawRotationHelper(bool val);
	void setRotationPoint(QPointF point1, QPointF point2);



protected:
	virtual void drawStuff(QPainter* painter);

	void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
	void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);

private:

	void drawSelection(QPainter* painter);
	void drawRotationHelper(QPainter* painter);


private:
	bool m_selected;
	
	int m_z;
	CLItemTransform m_animationTransform;
	GraphicsView* m_view;

	//rotation
	bool m_draw_rotation_helper;
	QPointF m_rotation_center;
	QPointF m_rotation_hand;

};


#endif //uvideo_wnd_h_1951