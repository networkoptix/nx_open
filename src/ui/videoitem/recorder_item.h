#ifndef recorder_item_h1421_h
#define recorder_item_h1421_h

#include <QMutex>
#include "custom_draw_button.h"

class CLRecorderDisplay;


class CLRecorderItem : public CLCustomBtnItem
{
	Q_OBJECT
public:
	CLRecorderItem(GraphicsView* view, int max_width, int max_height, QString name, QString tooltip);
	~CLRecorderItem();

	void setText(QString text);
	void setRecorderDisplay(CLRecorderDisplay* rec);
	CLRecorderDisplay* getRecorderDisplay() const;

protected:

	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);

	QString text() const;
protected:

	CLRecorderDisplay* mRecorder;
	mutable QMutex m_mutex;

};


#endif //recorder_item_h1421_h