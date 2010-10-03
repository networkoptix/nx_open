#ifndef custom_button_h_2001
#define custom_button_h_2001

#include "abstract_scene_item.h"

class CLCustomBtnItem : public CLAbstractSceneItem
{
public:
	CLCustomBtnItem(GraphicsView* view, int max_width, int max_height, QString name, QObject* handler, QString text, QString tooltipText);
	~CLCustomBtnItem();

protected:
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);


protected:
	QString mText;
	QPainterPath mRoundRectPath;
	QPainterPath mShadowRectPath;
};

#endif //custom_button_h_2001