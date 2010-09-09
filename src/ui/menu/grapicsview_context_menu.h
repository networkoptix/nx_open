#ifndef graphview_context_menu_1846
#define graphview_context_menu_1846

#include <QString>
#include <QList>
#include <QPointF>


class QState;
class QObject;
class TextButton;
class QGraphicsView;
class QParallelAnimationGroup;
class QGraphicsItem;

struct QViewMenuHandler
{
	virtual void OnMenuButton(void* owner, QString text) = 0;
};



class QViewMenu : public QViewMenuHandler
{
public:
	QViewMenu(void* owner, QViewMenuHandler* handler, QGraphicsView *view);
	virtual ~QViewMenu();

	void setOwner(void* owner);

	void addSeparator();
	void addMenu(QViewMenu*);

	void addItem(const QString& text);
	bool hasSuchItem(QGraphicsItem* item) const;

	void show(QPointF p);
	void hide();

private:
	void init_animatiom(QPointF p);
	void destroy_animation();

	void OnMenuButton(void* owner, QString text);

	QList<TextButton*> mItems;
	QGraphicsView *mView;
	bool mVisible;

	QParallelAnimationGroup *mAnim;

	void* mOwner;
	QViewMenuHandler* mHandler;
};

#endif // graphview_context_menu_1846