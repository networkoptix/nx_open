#include <QGraphicsItem>

class ButtonBackground;


class TextButton : public QGraphicsItem
{
public:
	enum ALIGNMENT {LEFT, RIGHT};
	enum BUTTONTYPE {SIDEBAR, PANEL, UP, DOWN};
	enum STATE {ON, OFF, HIGHLIGHT, DISABLED};

	TextButton(const QString &text, ALIGNMENT align = LEFT, int userCode = 0,
		QGraphicsScene *scene = 0, QGraphicsItem *parent = 0, BUTTONTYPE color = SIDEBAR);
	virtual ~TextButton();

	// overidden methods:
	virtual QRectF boundingRect() const;
	virtual void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget * = 0){};
	virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
	virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);
	virtual void mousePressEvent(QGraphicsSceneMouseEvent *event);
	virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

	
	void prepare();
	void setState(STATE state);
	void setMenuString(const QString &menu);
	void setDisabled(bool disabled);

	QString getText() const;

private:
	void setupButtonBg();
	
	void setupHoverText();


	ButtonBackground *bgOn;
	ButtonBackground *bgOff;
	ButtonBackground *bgHighlight;
	ButtonBackground *bgDisabled;

	BUTTONTYPE buttonType;
	ALIGNMENT alignment;
	QString buttonLabel;
	QString menuString;
	int userCode;
	QSize logicalSize;

	STATE state;
};
