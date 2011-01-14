#ifndef _abstarct_scene_item_h_1641
#define _abstarct_scene_item_h_1641

#include <QObject>
#include <QGraphicsItem>
#include "../animation/item_trans.h"
#include "subitem/abstract_sub_item.h"
#include "subitem/abstract_sub_item_container.h"


class GraphicsView;
class CLVideoWindowItem;
class CLAbstractSubItem;
class CLAbstractComplicatedItem;


class CLAbstractSceneItem : public CLAbstractSubItemContainer
{
	Q_OBJECT
	Q_PROPERTY(QPointF pos READ pos WRITE setPos)
	Q_PROPERTY(qreal rotation	READ getRotation WRITE setRotation)
public:

	enum CLSceneItemType {VIDEO, IMAGE, BUTTON, RECORDER, LAYOUT};

	CLAbstractSceneItem(GraphicsView* view, int max_width, int max_height,
						QString name="");

	virtual ~CLAbstractSceneItem();

	CLVideoWindowItem* toVideoItem() const;

	bool isEtitable() const;
	void setEditable(bool editable);

	// returns true of added
	

	QString getName() const;
	void setName(const QString& name);

	CLSceneItemType getType() const;
	void setType(CLSceneItemType t);

	CLAbstractComplicatedItem* getComplicatedItem() const;
	virtual void setComplicatedItem(CLAbstractComplicatedItem* complicatedItem);

	
	void setMaxSize(QSize size);
	QSize getMaxSize() const;

	virtual bool isZoomable() const;

	virtual QSize onScreenSize() const;

	virtual QRectF boundingRect() const;

	bool needUpdate() const
	{
		return m_needUpdate;
	}

	void needUpdate(bool val)
	{
		m_needUpdate = val;
	}


	virtual int height() const;
	virtual int width() const;

	virtual void setItemSelected(bool sel, bool animate = true, int delay = 0);
	bool isItemSelected() const;

	void zoom_abs(qreal z, int duration, int delay );
	void z_rotate_delta(QPointF center, qreal angle, int duration, int delay );
	void z_rotate_abs(QPointF center, qreal angle, int duration, int delay);

	qreal getZoom() const;

	qreal getRotation() const;
	void setRotation(qreal angle);

	void stop_animation();

	virtual void setFullScreen(bool full);
	bool isFullScreen() const;

	void setArranged(bool arr);
	bool isArranged() const;

	//====rotation=======
	void drawRotationHelper(bool val);

	
	void setRotationPointCenter(QPointF center);
	QPointF getRotationPointCenter() const;

	
	void setRotationPointHand(QPointF point);

public slots:
	

signals:
	void onPressed(CLAbstractSceneItem*);
	void onDoubleClick(CLAbstractSceneItem*);
	void onFullScreen(CLAbstractSceneItem*);
	void onSelected(CLAbstractSceneItem*);
	
protected:
	void drawShadow(QPainter* painter);

	void drawRotationHelper(QPainter* painter);

	virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
	virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);
	virtual void mouseReleaseEvent( QGraphicsSceneMouseEvent * event );
	virtual void mousePressEvent ( QGraphicsSceneMouseEvent * event );
	virtual void mouseDoubleClickEvent ( QGraphicsSceneMouseEvent * event );



protected:
	bool m_selected;
	bool m_arranged;
	bool m_fullscreen; // could be only if m_selected

	bool m_mouse_over;
	bool m_zoomOnhover;

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

	bool m_needUpdate;

	bool mEditable;

	CLAbstractComplicatedItem* m_complicatedItem;

};



#endif //_abstarct_scene_item_h_1641