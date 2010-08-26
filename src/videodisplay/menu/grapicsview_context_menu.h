#ifndef graphview_context_menu_1846
#define graphview_context_menu_1846

#include <QString>
#include <QList>
#include <QPointF>

class QObject;
class TextButton;
class QGraphicsView;

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
	QList<TextButton*> mItems;
	QGraphicsView *mView;

};

#endif // graphview_context_menu_1846