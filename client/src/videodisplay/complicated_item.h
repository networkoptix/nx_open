#ifndef colplicated_item_h_1706
#define colplicated_item_h_1706

#include "core/resource/resource.h"



class CLAbstractSceneItem;

class CLAbstractComplicatedItem
{
public:
	virtual ~CLAbstractComplicatedItem(){};

	virtual QnResourcePtr getDevice() const = 0;

	virtual void startDisplay() = 0;
	virtual void beforeStopDisplay() = 0;
	virtual void stopDisplay() = 0;

	virtual CLAbstractSceneItem* getSceneItem() const = 0;

};

#endif //colplicated_item_h_1706
