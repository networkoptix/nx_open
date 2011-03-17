#ifndef animated_show_h1223
#define animated_show_h1223

#include "abstract_animation.h"
class CLAbstractSceneItem;
class GraphicsView;

struct Show_helper : public QObject, public CLAbstractAnimation
{
	Q_OBJECT
public:
	explicit Show_helper(GraphicsView* view);
	virtual ~Show_helper();

	void setCenterPoint(QPointF center);
	QPointF getCenter() const;

	void setRadius(int radius);
	int getRadius() const;

	void setItems(QList<CLAbstractSceneItem*>* items);

	void setShowTime(bool showtime);
	bool isShowTime() const; // this is almost same as is running; but if show is about to start this value is diffrent

	virtual void stopAnimation();
	virtual bool isRuning() const;
	virtual QObject* object();

	int m_counrer;

public slots:

	void start(); // starts the show 

private slots:

	void onTimer();

private:

	QTimer mTimer;

	QPointF m_center;
	int m_radius;

	unsigned int m_value;
	bool m_positive_dir;

	QList<CLAbstractSceneItem*>* m_items;

	GraphicsView* m_view;

	bool m_showTime;
};

#endif //animated_show_h1223
