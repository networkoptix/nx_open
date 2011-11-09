#ifndef recorder_display_h_1347
#define recorder_display_h_1347

#include "videodisplay/complicated_item.h"


class QnResource;
class CLRecorderItem;

class CLRecorderDisplay : public CLLongRunnable, public CLAbstractComplicatedItem
{
public:
	CLRecorderDisplay(QnResourcePtr dev, CLRecorderItem* recitem);
	~CLRecorderDisplay();

	QnResourcePtr getDevice() const;

	virtual void startDispay();
	virtual void beforestopDispay();
	virtual void stopDispay();

	virtual CLAbstractSceneItem* getSceneItem() const;

	CLRecorderItem* getRecorderItem() const;

protected:
	void run();
private:
	QnResourcePtr mDev;
	CLRecorderItem* mRecitem;
};

#endif //recorder_display_h_1347
