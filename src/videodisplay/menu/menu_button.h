#include <QGraphicsItem>

class ButtonBackground;


class TextButton : public QGraphicsItem
{
public:
	enum STATE {NORMAL, HIGHLIGHT, PRESSED, DISABLED};

	TextButton(const QString &text);
		
	virtual ~TextButton();

	virtual QRectF boundingRect() const;
protected:
	// overidden methods:

	virtual void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget * = 0);
	virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
	virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);
	virtual void mousePressEvent(QGraphicsSceneMouseEvent *event);
	virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

	
	void setState(STATE state);
	QString getText() const;

private:
	QString text;
	STATE state;
};
