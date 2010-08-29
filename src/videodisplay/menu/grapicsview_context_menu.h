#ifndef graphview_context_menu_1846
#define graphview_context_menu_1846

#include <QString>
#include <QList>
#include <QPointF>


class QState;
class QObject;
class TextButton;
class QGraphicsView;
class QStateMachine;

struct QViewMenuHandler
{
	virtual void OnMenuButton(QObject* owner, QString* text) = 0;
};



class QViewMenu
{
public:
	QViewMenu(QObject* owner, QViewMenuHandler* handler, QGraphicsView *view);
	virtual ~QViewMenu();
	void addSeparator();
	void addMenu(QViewMenu*);

	void addItem(const QString& text);

	void show(QPointF p);
	void hide();

private:
	void init_state_machine(QPointF p);
	void destory_state_machine();

	QList<TextButton*> mItems;
	QGraphicsView *mView;
	bool mVisible;

	QStateMachine* mStates;
	QState *mRootState;
	QState *mNnormalState;
	QState *mOriginalState;


};

#endif // graphview_context_menu_1846