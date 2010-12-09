#ifndef serach_edit_item_h_2017
#define serach_edit_item_h_2017

#include "..\unmoved\abstract_animated_unmoved_item.h"

class QCompleter;
class QLineEdit;
class CLSearchComboBox;
class QGraphicsProxyWidget;

class CLSerachEditItem : public CLUnMovedOpacityItem
{
	Q_OBJECT
public:
	CLSerachEditItem(QString name, QGraphicsItem* parent, qreal normal_opacity, qreal active_opacity);
	~CLSerachEditItem();
	void resize();
protected:
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
	virtual QRectF boundingRect() const;
protected slots:
	void onTextChanged(QString text) ;
protected:
	int m_width;
	int m_height;

	CLSearchComboBox* m_lineEdit;
	
	QGraphicsProxyWidget* m_lineEditItem;


};

#endif //serach_edit_item_h_2017