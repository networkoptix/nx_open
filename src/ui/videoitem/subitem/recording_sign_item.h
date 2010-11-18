#ifndef recording_sign_item_h1519
#define recording_sign_item_h1519

#include "abstract_image_sub_item.h"
#include <QTimer>

class CLRecordingSignItem : public CLImgSubItem
{
	Q_OBJECT
public:
	CLRecordingSignItem(CLAbstractSceneItem* parent);
	~CLRecordingSignItem();
protected slots:
	void onTimer();
protected:
	static QTimer mTimer;
};

#endif //recording_sign_item_h1519