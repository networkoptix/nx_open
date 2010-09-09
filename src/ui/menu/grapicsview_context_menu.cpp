#include "grapicsview_context_menu.h"
#include "menu_button.h"
#include <QGraphicsView>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>


extern QFont buttonFont();

//======================================================

QViewMenu::QViewMenu(void* owner, QViewMenuHandler* handler, QGraphicsView *view):
mView(view),
mVisible(false),
mOwner(owner),
mHandler(handler)
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

void QViewMenu::setOwner(void* owner)
{
	mOwner = owner;
}

void QViewMenu::OnMenuButton(void* owner, QString text)
{
	hide();	
	if (mHandler)
		mHandler->OnMenuButton(mOwner, text);

}

void QViewMenu::addSeparator()
{

}

void QViewMenu::addItem(const QString& text)
{
	TextButton* item = new TextButton(text,  this);
	mItems.push_back(item);
}

void QViewMenu::init_animatiom(QPointF p)
{
	
	QParallelAnimationGroup *mAnim = new QParallelAnimationGroup;
	
	for (int i = 0; i < mItems.count(); ++i)
	{
		TextButton* item = mItems.at(i);

		qreal x = p.x();
		qreal y = p.y() + i*(item->boundingRect().height());

		QPropertyAnimation *anim = new QPropertyAnimation(mItems[i], "pos");

		anim->setStartValue(QPointF(mView->mapToScene(x,p.y())));
		anim->setEndValue(QPointF(mView->mapToScene(x,y)));

		anim->setDuration(300 + i * 25);
		//anim->setUpdateInterval(17);
		anim->setEasingCurve(QEasingCurve::InOutBack);
		mAnim->addAnimation(anim);

	}

	mAnim->start(QAbstractAnimation::DeleteWhenStopped);
}

void QViewMenu::destroy_animation()
{
	//mAnim->stop();
}


bool QViewMenu::hasSuchItem(QGraphicsItem* item) const
{
	for (int i = 0; i < mItems.size(); ++i)
	{
		TextButton* it = mItems.at(i);
		if (it==item)
			return true;
	}

	return false;
}

void QViewMenu::show(QPointF p)
{
	if (mVisible)
		return;

	int max_width = 0;
	int max_hight = 0;

	//==============================
	QFontMetrics fm(buttonFont());
	for (int i = 0; i < mItems.size(); ++i)
	{
		TextButton* item = mItems.at(i);
		QString text = item->getText();
		int pixelsWide = fm.width(text);
		int pixelsHigh = fm.height();

		if (pixelsWide > max_width)
			max_width = pixelsWide;

		if (pixelsHigh > max_hight)
			max_hight = pixelsHigh;
	}

	max_width+=30;
	max_hight+=10;

	//==============================
	int menu_width = max_width;
	int menu_height = max_hight*mItems.size();
	QRect viewRect = mView->viewport()->rect();

	if (p.x()+menu_width > viewRect.right())
		p.rx() = p.x()-menu_width;

	if (p.y()+menu_height> viewRect.bottom())
		p.ry() = p.y()-menu_height;



	//==============================

	for (int i = 0; i < mItems.count(); ++i)
	{
		TextButton* item = mItems.at(i);

		item->setTopBottom(false, false);

		if (i==0)
			item->setTopBottom(true, false);

		if (i==mItems.count()-1)
			item->setTopBottom(false, true);


		if (mItems.count()==1) // if it's the only item
			item->setTopBottom(true, true);


		item->setWidth(max_width);
		item->setHeight(max_hight);
		
		mView->scene()->addItem(item);

		qreal x = p.x();
		qreal y = p.y() + i*(item->boundingRect().height());

		item->setPos(mView->mapToScene(x,y));
	}

	mVisible = true;

	//init_animatiom(p);
	
}

void QViewMenu::hide()
{
	destroy_animation();
	if(!mVisible)
		return;

	for (int i = 0; i < mItems.size(); ++i)
	{
		TextButton* item = mItems.at(i);
		mView->scene()->removeItem(item);
		item->setState(TextButton::NORMAL, false);
	}
	

	mVisible = false;
}
