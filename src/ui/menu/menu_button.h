#ifndef menu_button_h_1746
#define menu_button_h_1746

#include <QGraphicsItem>

class ButtonBackground;
class QMouseEvent;
class QViewMenuHandler;


class TextButton : public QObject, public QGraphicsItem
{
	Q_OBJECT
	Q_PROPERTY(QPointF pos READ pos WRITE setPos)
public:
	enum STATE {NORMAL, HIGHLIGHT, PRESSED, DISABLED};

	TextButton(const QString &text, QObject* owner, QViewMenuHandler* handler);
		
	virtual ~TextButton();

	virtual QRectF boundingRect() const;

	void setWidth(int width);
	void setHeight(int height);

	QString getText() const;

	void setState(STATE state, bool update = true);

	void setTopBottom(bool top, bool bottom);

protected: 
	// overidden methods:

	virtual void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget * = 0);
	virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
	virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);
	virtual void mousePressEvent(QGraphicsSceneMouseEvent *event);

	virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

	virtual void mouseReleaseEvent(QMouseEvent *event);

	
	


private:
	QString text;
	STATE state;

	volatile int mWidth, mHight;

	QObject* mOwner;
	QViewMenuHandler* mHandler;

	bool mTop;
	bool mBottom;
};

#endif //menu_button_h_1746
