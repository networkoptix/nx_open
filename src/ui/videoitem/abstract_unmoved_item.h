#ifndef abstract_unmoved_item_h_1753
#define abstract_unmoved_item_h_1753

#include <QGraphicsItem>
class QGraphicsView;

class CLAbstractUnmovedItem : public QObject, public QGraphicsItem
{
	Q_OBJECT
public:
	CLAbstractUnmovedItem(QGraphicsView* view, QString name);
	virtual ~CLAbstractUnmovedItem();

	void setStaticPos(QPoint p); // sets pos in term of point view coordinates 
	void adjust(); // adjusts position and size of the item on the scene after scene transformation is done

protected:
	QString m_name;
	QGraphicsView* m_view;
	QPoint m_pos;

};


#endif //abstract_unmoved_item_h_1753


