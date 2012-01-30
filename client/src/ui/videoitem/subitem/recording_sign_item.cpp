#include "recording_sign_item.h"

#include "ui/style/skin.h"

QTimer CLRecordingSignItem::mTimer;

CLRecordingSignItem::CLRecordingSignItem(CLAbstractSubItemContainer* parent):
    CLImgSubItem(parent, Skin::path(QLatin1String("try/recording2.png")), RecordingSubItem, 0.6, 0.6, 400, 400)
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
    disconnect(&mTimer, SIGNAL(timeout()), this, SLOT(onTimer()));
}

void CLRecordingSignItem::onTimer()
{
    setVisible(!isVisible());
}
