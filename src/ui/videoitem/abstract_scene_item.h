#ifndef _abstarct_scene_item_h_1641
#define _abstarct_scene_item_h_1641

#include <QObject>
#include <QGraphicsItem>
#include "../animation/item_trans.h"


class GraphicsView;
class CLVideoWindowItem;

class CLAbstractSceneItem : public QObject, public QGraphicsItem
{
	Q_OBJECT
	Q_PROPERTY(QPointF pos READ pos WRITE setPos)
	Q_PROPERTY(qreal rotation	READ getRotation WRITE setRotation)
public:

	enum CLSceneItemType {VIDEO, IMAGE, BUTTON};

	CLAbstractSceneItem(GraphicsView* view, int max_width, int max_height,
						QString name="", QObject* handler=0);

	virtual ~CLAbstractSceneItem();

	CLVideoWindowItem* toVideoItem() const;

	CLSceneItemType getType() const;
	void setType(CLSceneItemType t);

	
	void setMaxSize(QSize size);
	QSize getMaxSize() const;

	virtual QRectF boundingRect() const;

	virtual int height() const;
	virtual int width() const;

	virtual void setSelected(bool sel, bool animate = true, int delay = 0);
	bool isSelected() const;

	void zoom_abs(qreal z, int duration, int delay = 0);
	void z_rotate_delta(QPointF center, qreal angle, int duration);
	void z_rotate_abs(QPointF center, qreal angle, int duration);

	qreal getZoom() const;

	qreal getRotation() const;
	void setRotation(qreal angle);

	void stop_animation();

	void setFullScreen(bool full);
	bool isFullScreen() const;

	void setArranged(bool arr);
	bool isArranged() const;

	//====rotation=======
	void drawRotationHelper(bool val);
	void setRotationPoint(QPointF point1, QPointF point2);


signals:
	void onPressed(QString);
	


protected:
	void drawShadow(QPainter* painter);

	void drawRotationHelper(QPainter* painter);

	virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
	virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);
	void mouseReleaseEvent( QGraphicsSceneMouseEvent * event );

protected:
	bool m_selected;
	bool m_arranged;
	bool m_fullscreen; // could be only if m_selected

	bool m_mouse_over;

	int m_z;
	CLItemTransform m_animationTransform;
	GraphicsView* m_view;

	int m_max_width, m_max_height;

	CLSceneItemType m_type;
	QString mName;

	//rotation
	bool m_draw_rotation_helper;
	QPointF m_rotation_center;
	QPointF m_rotation_hand;


};



#endif //_abstarct_scene_item_h_1641