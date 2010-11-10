#ifndef abstract_sub_item_h1031
#define abstract_sub_item_h1031

#include <QGraphicsItem>

class CLAbstractSubItem : public QObject, public QGraphicsItem
{
	Q_OBJECT
public:
	CLAbstractSubItem();
	~CLAbstractSubItem();

};

#endif //abstract_sub_item_h1031