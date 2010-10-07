#ifndef unmoved_pixture_button_h_1867
#define unmoved_pixture_button_h_1867

#include "abstract_unmoved_item.h"
#include <QTimer>
class QPropertyAnimation;

class CLUnMovedPixture : public CLAbstractUnmovedItem
{
	Q_OBJECT
public:
	CLUnMovedPixture(QGraphicsView* view, QString name, QString img, int max_width, int max_height, qreal z, qreal opacity);
	~CLUnMovedPixture();
	QRectF boundingRect() const;
protected:
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
protected:
	QPixmap m_img;
	int m_width;
	int m_height;
	qreal m_opacity;
};

//=============================================================================================

class CLUnMovedPixtureButton : public CLUnMovedPixture
{
	Q_OBJECT
	Q_PROPERTY(qreal opacity  READ opacity   WRITE setOpacity)
public:
	CLUnMovedPixtureButton(QGraphicsView* view, QString name, QString img, int max_width, int max_height, qreal z, qreal opacity);
signals:
	void onPressed(QString);
protected:
	virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
	virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);
	void mouseReleaseEvent( QGraphicsSceneMouseEvent * event );
	void mousePressEvent ( QGraphicsSceneMouseEvent * event );
protected slots:
	void stopAnimation();
protected:
	QPropertyAnimation* m_animation;

};

#endif //unmoved_pixture_button_h_1867