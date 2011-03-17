#ifndef custom_button_h_2001
#define custom_button_h_2001

#include "abstract_scene_item.h"

class CLCustomBtnItem : public CLAbstractSceneItem
{
public:
	CLCustomBtnItem(GraphicsView* view, int max_width, int max_height, QString name, QString text, QString tooltipText);
	~CLCustomBtnItem();

protected:
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);

protected:
	QPainterPath createRoundRectpath(int width, int height, int radius);

	void createPaths(int radius);
protected:

	QString mText;
	QPainterPath mRoundRectPath, mSmallRectPath;
	QPainterPath mShadowRectPath;
};

#endif //custom_button_h_2001
