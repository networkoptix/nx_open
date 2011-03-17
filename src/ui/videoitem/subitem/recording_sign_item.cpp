#include "recording_sign_item.h"

QTimer CLRecordingSignItem::mTimer;

CLRecordingSignItem::CLRecordingSignItem(CLAbstractSubItemContainer* parent):
CLImgSubItem(parent, ":/skin/try/recording2.png", RecordingSubItem, 0.6, 0.6, 400, 400)
{
	if (!mTimer.isActive()) // just for the first instance 
	{
		mTimer.setInterval(500);
		mTimer.start();
	}

	connect(&mTimer, SIGNAL(timeout()), this, SLOT(onTimer()));
}

CLRecordingSignItem::~CLRecordingSignItem()
{
	disconnect(&mTimer, SIGNAL(void timeout()), this, SLOT(void onTimer()));
}

void CLRecordingSignItem::onTimer()
{
	setVisible(!isVisible());
}