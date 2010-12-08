#ifndef serach_edit_item_h_2017
#define serach_edit_item_h_2017

#include "..\unmoved\abstract_animated_unmoved_item.h"

class QCompleter;
class QLineEdit;
class QComboBox;
class QGraphicsProxyWidget;

class CLSerachEditItem : public CLUnMovedOpacityItem
{
public:
	CLSerachEditItem(QString name, QGraphicsItem* parent, qreal normal_opacity, qreal active_opacity);
	~CLSerachEditItem();
	void resize();
protected:
	
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
	virtual QRectF boundingRect() const;
protected:
	int m_width;
	int m_height;

	QComboBox* m_lineEdit;
	
	QGraphicsProxyWidget* m_lineEditItem;


};

#endif //serach_edit_item_h_2017