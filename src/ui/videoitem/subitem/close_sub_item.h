#ifndef close_sub_item_h1207
#define close_sub_item_h1207


#include "abstract_image_sub_item.h"

class CLCloseSubItem : public CLAbstractImgSubItem
{
	Q_OBJECT
public:

	CLCloseSubItem(CLAbstractSceneItem* parent, qreal opacity, int max_width, int max_height);
	~CLCloseSubItem();
	virtual ItemType getType() const;
};

#endif //close_sub_item_h1207