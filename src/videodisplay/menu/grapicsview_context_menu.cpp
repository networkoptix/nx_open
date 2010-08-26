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
	TextButton* btn = new TextButton(text, TextButton::LEFT, TextButton::PANEL, mView->scene());
	

	mItems.push_back(btn);
}

void QViewMenu::show(QPointF p)
{
	for (int i = 0; i < mItems.size(); ++i)
	{
		TextButton* item = mItems.at(i);
		//item->setPos(p.x(), p.y() + i*item->boundingRect().height());
		item->setPos(p.x(), p.y() + i*200);
		item->setVisible(true);
	}
}

void QViewMenu::hide()
{
	for (int i = 0; i < mItems.size(); ++i)
	{
		TextButton* item = mItems.at(i);
		item->setVisible(false);
	}
	
}
