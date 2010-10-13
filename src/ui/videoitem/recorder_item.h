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

	bool needUpdate() const
	{
		return m_needUpdate;
	}

protected:

	void needUpdate(bool val)
	{
		m_needUpdate = val;
	}


	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);

	QString text() const;
protected:

	CLRecorderDisplay* mRecorder;
	bool m_needUpdate;
	mutable QMutex m_mutex;

};


#endif //recorder_item_h1421_h