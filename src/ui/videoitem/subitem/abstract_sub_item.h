#ifndef abstract_sub_item_h1031
#define abstract_sub_item_h1031

#include <QGraphicsItem>

class CLAbstractSceneItem;
class QPropertyAnimation;

class CLAbstractSubItem : public QObject, public QGraphicsItem
{
	Q_OBJECT
	Q_PROPERTY(qreal opacity  READ opacity   WRITE setOpacity)
public:
	enum ItemType {Close};
	CLAbstractSubItem(CLAbstractSceneItem* parent, qreal opacity);
	virtual ~CLAbstractSubItem();

	virtual ItemType getType() const = 0;

signals:
	void onPressed(CLAbstractSubItem*);
protected:
	virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
	virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);
	void mouseReleaseEvent( QGraphicsSceneMouseEvent * event );
	void mousePressEvent ( QGraphicsSceneMouseEvent * event );
protected slots:
	void stopAnimation();

protected:
	qreal m_opacity;
	QPropertyAnimation* m_animation;

};

#endif //abstract_sub_item_h1031