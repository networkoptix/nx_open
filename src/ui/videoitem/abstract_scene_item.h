#ifndef _abstarct_scene_item_h_1641
#define _abstarct_scene_item_h_1641

#include <QObject>
#include <QGraphicsItem>
#include "../animation/item_trans.h"


class GraphicsView;

class CLAbstractSceneItem : public QObject, public QGraphicsItem
{
	Q_OBJECT
public:

	enum CLSceneItemType {IMAGE, BUTTON};

	CLAbstractSceneItem(GraphicsView* view, int max_width, int max_height,
						QString name="", QObject* handler=0);

	virtual ~CLAbstractSceneItem();

	CLSceneItemType getType() const;
	void setType(CLSceneItemType t);

	void setMaxSize(QSize size);
	virtual void before_destroy();

	virtual QRectF boundingRect() const;

	virtual int height() const;
	virtual int width() const;

	void setSelected(bool sel, bool animate = true, int delay = 0);
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

signals:
	void onPressed(QString);
	


protected:
	void drawShadow(QPainter* painter);

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

};



#endif //_abstarct_scene_item_h_1641