#ifndef web_item_h_1425
#define web_item_h_1425

#include "abstract_scene_item.h"


class CLWebItem : public CLAbstractSceneItem
{
public:
	CLWebItem(GraphicsView* view, int max_width, int max_height, QString name="");
	~CLWebItem();
	void navigate(const QString& str);
protected:
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

	QPointF getBestSubItemPos(CLSubItemType type);
protected:

	QGraphicsProxyWidget* mWeb_item;
	QWebView m_web_widget;
};


#endif //web_item_h_1425