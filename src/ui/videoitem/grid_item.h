#ifndef CLGridItem_h_1841
#define CLGridItem_h_1841

class GraphicsView;

#include "abstract_scene_item.h"

class CLGridItem : public QObject, public QGraphicsItem
{
	Q_OBJECT

	Q_PROPERTY(qreal alpha  READ opacity   WRITE setAlpha)
public:
	CLGridItem(GraphicsView* view);
	~CLGridItem();
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
	QRectF boundingRect() const;

	// will make control visible and animate it from current opacity(normally zero), to 1.0.
	void show(int time_ms = 1000);

	// will animate control from current opacity( normally 1.0) to 0 and hide item 
	void hide(int time_ms = 1000);

	int alpha() const;
	void setAlpha(int val);

protected slots:
	void stopAnimation();

private:
	GraphicsView* m_view;
	int m_alpha;
	QPropertyAnimation* m_animation;

};

#endif // CLGridItem