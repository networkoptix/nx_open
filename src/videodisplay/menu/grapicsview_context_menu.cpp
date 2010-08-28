#include "grapicsview_context_menu.h"
#include "menu_button.h"
#include <QGraphicsView>

//======================================================

QViewMenu::QViewMenu(QObject* owner, QViewMenuHandler* handler, QGraphicsView *view):
mView(view)
{

}

QViewMenu::~QViewMenu()
{
	//hide();
	for(int i = 0; i < mItems.size(); ++i)
	{
		TextButton* btn = mItems.at(i);
		//delete btn;
	}
}

void QViewMenu::addSeparator()
{

}

void QViewMenu::addItem(const QString& text)
{
	TextButton* item = new TextButton(text);
	mItems.push_back(item);
}

void QViewMenu::show(QPointF p)
{
	for (int i = 0; i < mItems.size(); ++i)
	{
		TextButton* item = mItems.at(i);
		
		mView->scene()->addItem(item);
		item->setPos(p.x(), p.y() + i*(item->boundingRect().height()+5));
	}
	
}

void QViewMenu::hide()
{
	for (int i = 0; i < mItems.size(); ++i)
	{
		TextButton* item = mItems.at(i);
		mView->scene()->removeItem(item);
	}
	
}
