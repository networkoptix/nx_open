#ifndef abstract_sub_item_container_h1031
#define abstract_sub_item_container_h1031

#include <QGraphicsItem>
#include <QObject>

class CLAbstractSubItem;

class CLAbstractSubItemContainer : public QObject, public QGraphicsItem
{
	Q_OBJECT
public:
	CLAbstractSubItemContainer(QGraphicsItem* parent):
	QGraphicsItem(parent)
	{

	}

	~CLAbstractSubItemContainer(){}

	protected slots:
		virtual void onSubItemPressed(CLAbstractSubItem* subitem){};

};


#endif //abstract_sub_item_container_h1031