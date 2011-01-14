#ifndef recorder_item_h1421_h
#define recorder_item_h1421_h

#include <QMutex>
#include "custom_draw_button.h"

class CLRecorderDisplay;
class LayoutContent;

class CLRecorderItem : public CLCustomBtnItem
{
	Q_OBJECT
public:
	CLRecorderItem(GraphicsView* view, int max_width, int max_height, QString name, QString tooltip);
	~CLRecorderItem();

	void setRefContent(LayoutContent* cont);
	LayoutContent* getRefContent() const;

	void setText(QString text);
	CLRecorderDisplay* getRecorderDisplay() const;

protected:
	QPointF getBestSubItemPos(CLSubItemType type);
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);

	QString text() const;
protected:

	
	LayoutContent* mContent; //this item references to this content
	mutable QMutex m_mutex;

};


#endif //recorder_item_h1421_h