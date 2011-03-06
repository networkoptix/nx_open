#ifndef abstract_unmoved_item_h_1753
#define abstract_unmoved_item_h_1753

#include "..\subitem\abstract_sub_item_container.h"

class QGraphicsView;

class CLAbstractUnmovedItem : public CLAbstractSubItemContainer
{
	Q_OBJECT
public:
	CLAbstractUnmovedItem(QString name="", QGraphicsItem* parent = 0);
	virtual ~CLAbstractUnmovedItem();

	void setStaticPos(QPoint p); // sets pos in term of point view coordinates 
	void adjust(); // adjusts position and size of the item on the scene after scene transformation is done

	QString getName() const;

    virtual void hide(int duration) = 0;
    virtual void show(int duration) = 0;

protected:
	QString m_name;
	QGraphicsView* m_view;
	QPoint m_pos;

};

#endif //abstract_unmoved_item_h_1753

