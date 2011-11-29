#include "recorder_display.h"
#include "ui/videoitem/recorder_item.h"
#include "utils/common/sleep.h"



CLRecorderDisplay::CLRecorderDisplay(QnResourcePtr dev, CLRecorderItem* recitem):
mDev(dev),
mRecitem(recitem)
{
	recitem->setComplicatedItem(this);
}

CLRecorderDisplay::~CLRecorderDisplay()
{
	stop();
}

QnResourcePtr CLRecorderDisplay::getDevice() const
{
	return mDev;
}

void CLRecorderDisplay::startDisplay()
{
	start();
}

void CLRecorderDisplay::beforeStopDisplay()
{
	pleaseStop();
}

void CLRecorderDisplay::stopDisplay()
{
	stop();
}

CLAbstractSceneItem* CLRecorderDisplay::getSceneItem() const
{
	return mRecitem;
}

CLRecorderItem* CLRecorderDisplay::getRecorderItem() const
{
	return mRecitem;
}

void CLRecorderDisplay::run()
{
	while(!needToStop())
	{
		QString text = mDev->toString();
		mRecitem->setText(text);

		//=== 0.5 sec delay===
		for (int i = 0; i < 5; ++i)
		{
			if (needToStop())
				break;

			QnSleep::msleep(100);
		}

	}
}
