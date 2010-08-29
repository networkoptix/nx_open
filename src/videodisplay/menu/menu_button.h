#include <QGraphicsItem>

class ButtonBackground;


class TextButton : public QObject, public QGraphicsItem
{
	Q_OBJECT
	Q_PROPERTY(QPointF pos READ pos WRITE setPos)
public:
	enum STATE {NORMAL, HIGHLIGHT, PRESSED, DISABLED};

	TextButton(const QString &text);
		
	virtual ~TextButton();

	virtual QRectF boundingRect() const;

	void setWidth(int width);
	void setHeight(int height);

	QString getText() const;

protected: 
	// overidden methods:

	virtual void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget * = 0);
	virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
	virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);
	virtual void mousePressEvent(QGraphicsSceneMouseEvent *event);
	virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

	
	void setState(STATE state);


private:
	QString text;
	STATE state;

	volatile int mWidth, mHight;
};
