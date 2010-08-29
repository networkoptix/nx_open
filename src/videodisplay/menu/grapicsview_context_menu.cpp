#include "grapicsview_context_menu.h"
#include "menu_button.h"
#include <QGraphicsView>
#include <QState>
#include <QStateMachine>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QAbstractTransition>
#include <QTimer>
#include <QSignalTransition>

extern QFont buttonFont();

//======================================================

QViewMenu::QViewMenu(QObject* owner, QViewMenuHandler* handler, QGraphicsView *view):
mView(view),
mVisible(false)
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

void QViewMenu::init_state_machine(QPointF p)
{
	mStates = new QStateMachine;
	mRootState = new QState;
	mNnormalState = new QState(mRootState);
	mOriginalState = new QState(mRootState);

	mStates->addState(mRootState);
	mStates->setInitialState(mRootState);
	mRootState->setInitialState(mOriginalState);

	for (int i = 0; i < mItems.count(); ++i)
	{
		TextButton* item = mItems.at(i);

		qreal x = p.x();
		qreal y = p.y() + i*(item->boundingRect().height());
		//item->setPos(mView->mapToScene(x,y));

		mNnormalState->assignProperty(item, "pos",	mView->mapToScene(x,y));

		mOriginalState->assignProperty(item, "pos",	mView->mapToScene(x,p.y()));
	}

	QParallelAnimationGroup *group = new QParallelAnimationGroup;

	for (int i = 0; i < mItems.count(); ++i) 
	{
		QPropertyAnimation *anim = new QPropertyAnimation(mItems[i], "pos");
		anim->setDuration(550 + i * 25);
		anim->setEasingCurve(QEasingCurve::InOutBack);
		group->addAnimation(anim);
	}

	QTimer* timer = new QTimer;
	timer->start(50);
	timer->setSingleShot(true);
	QSignalTransition *trans = mRootState->addTransition(timer, SIGNAL(timeout()), mNnormalState);
	trans->addAnimation(group);

	mStates->start();



}

void QViewMenu::destory_state_machine()
{

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

	for (int i = 0; i < mItems.size(); ++i)
	{
		TextButton* item = mItems.at(i);
		item->setWidth(max_width);
		item->setHeight(max_hight);
		
		mView->scene()->addItem(item);

		qreal x = p.x();
		qreal y = p.y();
		item->setPos(mView->mapToScene(x,y));
	}

	mVisible = true;

	init_state_machine(p);
	
}

void QViewMenu::hide()
{

	if(!mVisible)
		return;

	for (int i = 0; i < mItems.size(); ++i)
	{
		TextButton* item = mItems.at(i);
		mView->scene()->removeItem(item);
	}
	

	mVisible = false;
}
