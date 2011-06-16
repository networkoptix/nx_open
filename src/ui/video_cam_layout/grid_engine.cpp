#include "grid_engine.h"
#include "device/device_video_layout.h"
#include "ui/videoitem/abstract_scene_item.h"
#include "ui/videoitem/video_wnd_item.h"
#include <math.h>
#include "layout_content.h"

CLGridEngine::CLGridEngine()
{

}

CLGridEngine::~CLGridEngine()
{

}

void CLGridEngine::setSettings(const CLGridSettings& sett)
{
	m_settings = sett;
}

CLGridSettings& CLGridEngine::getSettings()
{
	return m_settings;
}

void CLGridEngine::setLayoutContent(LayoutContent* sl)
{
	m_scene_layout = sl;
}

void CLGridEngine::clarifyLayoutContent()
{
    CLUserGridSettings& ugs = m_scene_layout->getGridSettings();
    ugs.item_distance = m_settings.item_distance*CLUserGridSettings::scale_factor;
    ugs.optimal_ratio = m_settings.optimal_ratio*CLUserGridSettings::scale_factor;
    ugs.max_rows = m_settings.max_rows;
    
    foreach (CLAbstractSceneItem* item, *(m_settings.items))
    {
        QString name = item->getName();
        LayoutItem* litem = m_scene_layout->getItemByname(name);

        if (!litem)
            continue;

        CLBasicLayoutItemSettings& sett = litem->getBasicSettings();
        sett.coordType = CLBasicLayoutItemSettings::Slots;
        getItemSlotPos(item, sett.pos_x, sett.pos_y);
        sett.angle = item->getRotation();

    }
}


QPoint CLGridEngine::posFromItemSettings(const CLBasicLayoutItemSettings& sett) const
{
    if (sett.coordType==CLBasicLayoutItemSettings::Pixels)
        return QPoint(sett.pos_x, sett.pos_y);

    if (sett.coordType==CLBasicLayoutItemSettings::Slots)
        return posFromSlot(sett.pos_x, sett.pos_y);

    return QPoint(m_settings.left, m_settings.top);
}

QSize CLGridEngine::calcDefaultMaxItemSize(const CLDeviceVideoLayout* layout) const
{
	if (layout==0)
		return QSize(m_settings.slot_width, m_settings.slot_height);

	qreal dlw = layout->width();
	qreal dlh = layout->height();

	if (dlw == 4 && dlh == 1)
		dlw = 3;

	return QSize(m_settings.slot_width*(dlw + m_settings.item_distance*(dlw-1)), m_settings.slot_height*(dlh + m_settings.item_distance*(dlh-1)));

}

QSize CLGridEngine::getItemMaxSize(const CLAbstractSceneItem* item) const
{
	if (item->toVideoItem())
		return calcDefaultMaxItemSize(item->toVideoItem()->getVideoLayout());

	return QSize(m_settings.slot_width, m_settings.slot_height);
}

bool CLGridEngine::isSpaceAvalable() const
{
    int count = 0;

    
    foreach(CLAbstractSceneItem* item, *m_settings.items)
    {
        if (item->toVideoItem())
            count += item->toVideoItem()->getVideoLayout()->numberOfChannels();
        else
            ++count;
    }

	return count < m_settings.max_items;
}

QRect CLGridEngine::gridSlotRect() const
{
	CLAbstractSceneItem* item =  getVeryLeftItem();

	if (!item) // nos single video on this lay out
		return QRect(0,0,0,0);

	int left = item->mapToScene(item->boundingRect().topLeft()).x();

	item =  getVeryTopItem();
	int top = item->mapToScene(item->boundingRect().topLeft()).y();

	item =  getVeryRightItem();
	int right = item->mapToScene(item->boundingRect().bottomRight()).x();

	item =  getVeryBottomItem();
	int bottom = item->mapToScene(item->boundingRect().bottomRight()).y();

	int slot_left_x, slot_top_y;
	slotFromPos(QPoint(left, top), slot_left_x, slot_top_y);

	int slot_right_x, slot_bottom_y;
	slotFromPos(QPoint(right, bottom), slot_right_x, slot_bottom_y);

	return QRect(QPoint(slot_left_x, slot_top_y), QPoint(slot_right_x, slot_bottom_y));

}

QRect CLGridEngine::getGridRect() const
{

	CLAbstractSceneItem* item =  getVeryLeftItem();

	if (!item) // nos single video on this lay out
		return QRect(m_settings.left, m_settings.top, 10, 10 );

	int left = item->mapToScene(item->boundingRect().topLeft()).x();

	item =  getVeryTopItem();
	int top = item->mapToScene(item->boundingRect().topLeft()).y();

	item =  getVeryRightItem();
	int right = item->mapToScene(item->boundingRect().bottomRight()).x();

	item =  getVeryBottomItem();
	int bottom = item->mapToScene(item->boundingRect().bottomRight()).y();

	QRect video_rect(QPoint(left, top), QPoint(right, bottom) );

	video_rect.adjust(-m_settings.item_distance*m_settings.slot_width/2, -m_settings.item_distance*m_settings.slot_height, 
						m_settings.item_distance*m_settings.slot_width/2, m_settings.item_distance*m_settings.slot_height);

	CLRectAdjustment adjustment = m_scene_layout->getRectAdjustment();
	video_rect.adjust(adjustment.x1, adjustment.y1, adjustment.x2, adjustment.y2);

	return video_rect;
}

QList<CLIdealItemPos> CLGridEngine::calcArrangedPos() const
{
	QList<CLIdealItemPos> result;

	if (m_settings.items->count()==0)
		return result;

	// fist of all lets sort items; big one should go first 
	QList<CLAbstractSceneItem*> sorted_items = *(m_settings.items);
	while(1)
	{
		bool swaped = false;

		for (int i = 0; i < sorted_items.count()-1; ++i)
		{
			CLAbstractSceneItem* item_curr = sorted_items.at(i);
			QSize item_current_size =  getItemMaxSize(item_curr);
			int cur_size = qMax( slotsW(item_current_size.width()),  slotsH(item_current_size.height())); 

			CLAbstractSceneItem* item_next = sorted_items.at(i+1);
			QSize item_next_size =  getItemMaxSize(item_next);
			int next_size = qMax( slotsW(item_next_size.width()),  slotsH(item_next_size.height()));

			if (cur_size < next_size)
			{
				sorted_items.swap(i, i+1);			
				swaped = true;
			}

		}

		if (!swaped)
			break;
	}

	//==========sorted=====================
	int current_grid_width = 0;
	int current_grid_height = 0;

	foreach(CLAbstractSceneItem* item, sorted_items)
	{
		QSize item_size =  getItemMaxSize(item);

		int x, y;
		getNextAvailablePos_helper(item_size, x,y, current_grid_width, current_grid_height, result);

		CLIdealItemPos ipos;

		int width = item->width();
		int height = item->height();

		x += (item_size.width() - width)/2;
		y += (item_size.height() - height)/2;

		ipos.pos = QPoint(x,y) ;
		ipos.item = item;
		result.push_back(ipos);

		int item_slots_width = slotsW(item_size.width());
		int item_slots_height = slotsH(item_size.height());

		int slot_x, slot_y;
		slotFromPos(ipos.pos, slot_x, slot_y);

		current_grid_width = qMax(current_grid_width, slot_x + item_slots_width);
		current_grid_height = qMax(current_grid_height, slot_y + item_slots_height);

	}

	return result;
}

bool CLGridEngine::getNextAvailablePos(QSize size, int &x_pos, int &y_pos) const
{
	// we should try to insert item at the end of each row; rows index = [0; min( last_raw+1, m_settings.max_rows];
	// and see witch way it's closer to 4:3 ratio.

	int item_slots_width = slotsW(size.width());
	int item_slots_height = slotsH(size.height());

	int max_row_to_try = 0;
	int max_column_to_try = 0;

	int current_grid_width = 0;
	int current_grid_height = 0;

	CLAbstractSceneItem* item = getVeryBottomItem();
	if (item)
	{
		QPointF p = item->mapToScene(item->boundingRect().bottomRight());
		int slot_x, slot_y;
		slotFromPos(QPoint(p.x(),p.y()), slot_x, slot_y);
		current_grid_height = slot_y + 1;
		max_row_to_try = qMin(current_grid_height, m_settings.max_rows - item_slots_height ); //+

		item = getVeryRightItem();
		p = item->mapToScene(item->boundingRect().topRight());
		slotFromPos(QPoint(p.x(),p.y()), slot_x, slot_y);
		current_grid_width = slot_x + 1;
		max_column_to_try = current_grid_width;
	}
	else
	{
		QPoint pos = posFromSlot(0,0);
		x_pos = pos.x();
		y_pos = pos.y();
		return true;
	}

	// first of all lets check if we can add item without changing grid aspect ratio 
	for (int y = 0; y <= current_grid_height - item_slots_height; ++y)
	{
		for (int x = 0; x <= current_grid_width - item_slots_width; ++x)
		{
			if (!isSlotAvailable(x,y,size))
				continue;

			QPoint pos = posFromSlot(x,y);
			x_pos = pos.x();
			y_pos = pos.y();
			return true;

		}
	}

	qreal best_ratio = 0xfffff;
	int best_x_slot_pos = 0;
	int best_y_slot_pos = 0;

	bool last_column_taken = false;
	bool last_row_taken = false;

	for (int y = 0; y <= max_row_to_try; ++y)
	{
		for (int x = 0; x <= max_column_to_try; ++x)
		{

			if (x <= max_column_to_try - item_slots_width && y <= max_row_to_try - item_slots_height)
				continue; // already checked 

			if (x==max_column_to_try && last_column_taken)
				continue;

			if (y==max_row_to_try && last_row_taken)
				continue;

			if (!isSlotAvailable(x,y,size))
				continue;

			int new_grid_width = qMax(current_grid_width, x + item_slots_width);
			int new_grid_height = qMax(current_grid_height, y + item_slots_height);

			qreal new_aspect = (qreal)(new_grid_width)*m_settings.slot_width/(new_grid_height*m_settings.slot_height);
			new_aspect = qAbs(new_aspect - m_settings.optimal_ratio);

			if (new_aspect < best_ratio - 1e-7)
			{
				best_ratio = new_aspect;
				best_x_slot_pos = x;
				best_y_slot_pos = y;

				if (x==max_column_to_try) // to take fist position in the row 
					last_column_taken = true;

				if (y==max_row_to_try)
					last_row_taken = true;

			}

		}
	}

	QPoint pos = posFromSlot(best_x_slot_pos, best_y_slot_pos);
	x_pos = pos.x();
	y_pos = pos.y();
	return true;

}

QPoint CLGridEngine::adjustedPosForItem(CLAbstractSceneItem* item) const 
{
	int slot_x, slot_y;
	getItemSlotPos(item, slot_x, slot_y);
	return adjustedPosForSlot(item, slot_x,  slot_y);
}

QPoint CLGridEngine::adjustedPosForSlot(CLAbstractSceneItem* item, int slot_x, int slot_y) const 
{
	QPoint new_p  = posFromSlot( slot_x , slot_y ); 

	int width = item->width();
	int height = item->height();

	QSize max_size = item->getMaxSize();

	new_p.rx() += (max_size.width() - width)/2;
	new_p.ry() += (max_size.height() - height)/2;

	return new_p;
}

void CLGridEngine::getItemSlotPos(CLAbstractSceneItem* item, int& slot_x, int& slot_y) const
{
	QPointF p = item->mapToScene(item->boundingRect().center());
	p-=QPointF(item->width()/2 - 1e-7, item->height()/2 - 1e-7); // this addition as not good at all; due to item zoom topLeft pos might be shifted to diff slot; case1

	slotFromPos( QPoint(p.x(),p.y()), slot_x,  slot_y);
}

void CLGridEngine::adjustItem(CLAbstractSceneItem* item) const
{
	QPoint new_p = adjustedPosForItem(item);
	item->setPos(new_p);
}

CLAbstractSceneItem*  CLGridEngine::getCenterWnd() const
{
	QPoint mass_cnt = getMassCenter();

	unsigned int min_distance = 0xffffffff;

	CLAbstractSceneItem* result = 0;

	foreach (CLAbstractSceneItem* item, *(m_settings.items))
	{
		QPointF p = item->mapToScene(item->boundingRect().center());
		int dx = mass_cnt.x() - p.x();
		int dy = mass_cnt.y() - p.y();

		unsigned int d = dx*dx + dy*dy;

		if (d < min_distance)
		{
			min_distance = d;
			result = item;
		}

	}

	return result;
}

CLAbstractSceneItem* CLGridEngine::getItemToSwapWith(CLAbstractSceneItem* item) const
{

	QRectF item_rect = item->sceneBoundingRect();

	QSize item_max_size = getItemMaxSize(item);

	foreach (CLAbstractSceneItem* swapitem, *(m_settings.items))
	{
		if (item == swapitem)
			continue;

		QSize swapitem_max_size = getItemMaxSize(swapitem);
		if (swapitem_max_size!=item_max_size)
			continue;

		QRectF swapitem_rect = swapitem->sceneBoundingRect();

		if (!swapitem_rect.intersects(item_rect))
			continue;

		QRectF intersection = swapitem_rect.intersected(item_rect);

		qreal intersection_square = intersection.width()*intersection.height();
		qreal item_rect_square = item_rect.width()*item_rect.height();
		qreal swapitem_rect_square = swapitem_rect.width()*swapitem_rect.height();

		if (intersection_square  > 0.6* qMin(item_rect_square,swapitem_rect_square))
			return swapitem;

	}

	return 0;
}

bool CLGridEngine::canBeDropedHere(CLAbstractSceneItem* item) const
{

	int left_slot, top_slot;
	getItemSlotPos(item, left_slot, top_slot);

	QSize item_size(item->width(), item->height());

	if (!isSlotAvailable(left_slot, top_slot, item_size, item))
		return false;

	int item_slots_width = slotsW(item_size.width());
	int item_slots_height = slotsH(item_size.height());

	QRect slots_rect( slotPos(left_slot, top_slot),  slotPos(left_slot + item_slots_width, top_slot + item_slots_height) );
	QRectF item_rect = item->sceneBoundingRect();

	return slots_rect.contains(item_rect.toAlignedRect(), true);
}

CLAbstractSceneItem* CLGridEngine::getNextLeftItem(const CLAbstractSceneItem* curr) const
{
	return next_item_helper(curr, 3, 1);
}

CLAbstractSceneItem* CLGridEngine::getNextRightItem(const CLAbstractSceneItem* curr) const
{
	return next_item_helper(curr, 1, 3);
}

CLAbstractSceneItem* CLGridEngine::getNextTopItem(const CLAbstractSceneItem* curr) const
{
	return next_item_helper(curr, 0, 2);
}

CLAbstractSceneItem* CLGridEngine::getNextBottomItem(const CLAbstractSceneItem* curr) const
{
	return next_item_helper(curr, 2, 0);
}

//===========private=========================================================

CLAbstractSceneItem* CLGridEngine::getVeryLeftItem() const
{
	CLAbstractSceneItem* result = 0;

	unsigned int min_val = 0xffffffff; // very big value

	foreach (CLAbstractSceneItem* item, *(m_settings.items))
	{
		QPointF p = item->mapToScene(item->boundingRect().topLeft());
		if (p.x() < min_val)
		{
			min_val = p.x();
			result = item;
		}
	}

	return result;
}

CLAbstractSceneItem* CLGridEngine::getVeryTopItem() const
{
	CLAbstractSceneItem* result = 0;

	unsigned int min_val = 0xffffffff; // very big value

	foreach (CLAbstractSceneItem* item, *(m_settings.items))
	{
		QPointF p = item->mapToScene(item->boundingRect().topLeft());
		if (p.y() < min_val)
		{
			min_val = p.y();
			result = item;
		}
	}

	return result;
}

CLAbstractSceneItem* CLGridEngine::getVeryRightItem() const
{
	CLAbstractSceneItem* result = 0;

	unsigned int max_val = 0;

	foreach (CLAbstractSceneItem* item, *(m_settings.items))
	{
		QPointF p = item->mapToScene(item->boundingRect().bottomRight());
		if (p.x() >= max_val)
		{
			max_val = p.x();
			result = item;
		}
	}

	return result;
}

CLAbstractSceneItem* CLGridEngine::getVeryBottomItem() const
{
	CLAbstractSceneItem* result = 0;

	unsigned int max_val = 0;

	foreach (CLAbstractSceneItem* item, *(m_settings.items))
	{
		QPointF p  = item->mapToScene(item->boundingRect().bottomRight());
		if (p.y() >= max_val)
		{
			max_val = p.y();
			result = item;
		}
	}

	return result;

}

QPoint CLGridEngine::getMassCenter() const
{
	if (m_settings.items->count()==0)
		return QPoint(m_settings.left, m_settings.top);

	QPointF result(0.0,0.0);

	foreach (CLAbstractSceneItem* item, *(m_settings.items))
	{
		QPointF p  = item->mapToScene(item->boundingRect().center());
		result+=p;
	}

	return QPoint(result.x()/m_settings.items->size(), result.y()/m_settings.items->size());

}

CLAbstractSceneItem* CLGridEngine::next_item_helper(const CLAbstractSceneItem* curr, int dir_c, int dir_f) const
{
	if (!curr)
		return 0;

	if (m_settings.items->size()==1)
		return const_cast<CLAbstractSceneItem*>(curr);

	QPointF cP = curr->mapToScene(curr->boundingRect().center());

	//looking for closest left wnd 
	int dx, dy;
	qreal min_distance = 1e+20, max_distance = 0, distance;

	CLAbstractSceneItem* result = 0;

	foreach (CLAbstractSceneItem* item, *(m_settings.items))
	{
		QPointF p = item->mapToScene(item->boundingRect().center());

		if (next_item_helper_get_quarter(cP, p)!=dir_c)
			continue;

		dx = cP.x() - p.x();
		dy = cP.y() - p.y();

		distance = (qreal)dx*dx + dy*dy;

		if (distance < min_distance)
		{
			min_distance = distance;
			result = item;
		}
	}

	if (result)
		return result;

	// need to find very right wnd

	foreach (CLAbstractSceneItem* item, *(m_settings.items))
	{
		QPointF p = item->mapToScene(item->boundingRect().center());

		if (next_item_helper_get_quarter(cP, p)!=dir_f)
			continue;

		dx = cP.x() - p.x();
		dy = cP.y() - p.y();

		distance = dx*dx + dy*dy;

		if (distance > max_distance)
		{
			max_distance = distance;
			result = item;
		}
	}

	if (result)
		return result;

	return const_cast<CLAbstractSceneItem*>(curr);

}

int CLGridEngine::next_item_helper_get_quarter(const QPointF& current, const QPointF& other) const
{
	/*

	\	/
	 \ /
	  *
	 / \
	/	\

	up(0)
	right(1)
	down(2)
	left(3)
	/**/

	int dx = other.x() - current.x();
	int dy = other.y() - current.y();

	if ((qreal)dx*dx + dy*dy<8)
		return -1; // if distance is so small we should

	if (dy<0) // might be up, left right
	{
		if (dx>0) // up or right
		{
			if ((-dy) > dx )
				return 0;

			return 1;
		}
		else // up or left
		{
			if ((-dy) > (-dx))
				return 0;

			return 3;
		}
	}
	else // down, left, right
	{
		if (dx>0) // right, down
		{
			if (dy > dx)
				return 2;

			return 1;
		}
		else // left bottom 
		{
			if (dy > (-dx))
				return 2;

			return 3;
		}
	}

	return -1;

}

void CLGridEngine::slotFromPos(QPoint p, int& slot_x, int& slot_y) const
{
	int x = p.x() - m_settings.left;
	int y = p.y() - m_settings.top;

	slot_x = floor(x/( m_settings.slot_width*(1+m_settings.item_distance - 1e-10) ));
	slot_y = floor(y/( m_settings.slot_height*(1+m_settings.item_distance - 1e-10) ));
}

QPoint CLGridEngine::slotPos(int slot_x, int slot_y) const
{
	return QPoint(m_settings.left + slot_x*m_settings.slot_width*(1+m_settings.item_distance), 
				  m_settings.top + slot_y*m_settings.slot_height*(1+m_settings.item_distance));
}

QPoint CLGridEngine::posFromSlot(int slot_x, int slot_y) const
{
	return slotPos(slot_x, slot_y) + QPoint(m_settings.insideSlotShiftH(), m_settings.insideSlotShiftV());
}

bool CLGridEngine::isSlotAvailable(int slot_x, int slot_y, QSize size, CLAbstractSceneItem* ignoreItem) const
{
	QPoint slPos = posFromSlot(slot_x, slot_y);
	QRectF new_wnd_rect(slPos, size);

	foreach (CLAbstractSceneItem* item, *(m_settings.items))
	{
		if (ignoreItem==item)
			continue;

		QRectF wnd_rect = item->mapToScene(item->boundingRect()).boundingRect();

		if (new_wnd_rect.intersects(wnd_rect))
			return false;		
	}

	return true;
}

// how many slots window occupies 
int CLGridEngine::slotsW(int width) const
{
	return ceil( (qreal(width)/m_settings.slot_width + m_settings.item_distance)/(1+m_settings.item_distance) - 1e-7);
}

int CLGridEngine::slotsH(int height) const
{
	return ceil( (qreal(height)/m_settings.slot_height + m_settings.item_distance)/(1+m_settings.item_distance) - 1e-7);
}

bool CLGridEngine::isSlotAvailable_helper(int slot_x, int slot_y, QSize size, QList<CLIdealItemPos>& arranged) const
{

	QRect r1(slot_x, slot_y, slotsW(size.width()), slotsH(size.height()) );

	foreach(CLIdealItemPos ipos, arranged)
	{
		QPoint pos = ipos.pos;

		int item_min_x, item_min_y;
		slotFromPos(pos, item_min_x, item_min_y);
		QSize item_size =  getItemMaxSize(ipos.item);

		QRect r2(item_min_x, item_min_y, slotsW(item_size.width()), slotsH(item_size.height()));

		if (r1.intersects(r2))
			return false;

	}

	return true;

}

bool CLGridEngine::getNextAvailablePos_helper(QSize size, int &x_pos, int &y_pos, int current_grid_width, int current_grid_height, QList<CLIdealItemPos>& arranged) const
{
	// we should try to insert item at the end of each row; rows index = [0; min( last_raw+1, m_settings.max_rows];
	// and see witch way it's closer to 4:3 ratio.

	int item_slots_width = slotsW(size.width());
	int item_slots_height = slotsH(size.height());

	int max_row_to_try = 0;
	int max_column_to_try = 0;

	max_row_to_try = qMin(current_grid_height, m_settings.max_rows - item_slots_height ); //+
	max_column_to_try = current_grid_width;

	// first of all lets check if we can add item without changing grid aspect ratio 
	for (int y = 0; y <= current_grid_height - item_slots_height; ++y)
	{
		for (int x = 0; x <= current_grid_width - item_slots_width; ++x)
		{
			if (!isSlotAvailable_helper(x,y,size, arranged))
				continue;

			QPoint pos = posFromSlot(x,y);
			x_pos = pos.x();
			y_pos = pos.y();
			return true;

		}
	}

	qreal best_ratio = 0xfffff;
	int best_x_slot_pos = 0;
	int best_y_slot_pos = 0;

	bool last_column_taken = false;
	bool last_row_taken = false;

	for (int y = 0; y <= max_row_to_try; ++y)
	{
		for (int x = 0; x <= max_column_to_try; ++x)
		{

			if (x <= max_column_to_try - item_slots_width && y <= max_row_to_try - item_slots_height)
				continue; // already checked 

			if (x==max_column_to_try && last_column_taken)
				continue;

			if (y==max_row_to_try && last_row_taken)
				continue;

			if (!isSlotAvailable_helper(x,y,size, arranged))
				continue;

			int new_grid_width = qMax(current_grid_width, x + item_slots_width);
			int new_grid_height = qMax(current_grid_height, y + item_slots_height);

			qreal new_aspect = (qreal)(new_grid_width)*m_settings.slot_width/(new_grid_height*m_settings.slot_height);
			new_aspect = qAbs(new_aspect - m_settings.optimal_ratio);

			if (new_aspect < best_ratio)
			{
				best_ratio = new_aspect;
				best_x_slot_pos = x;
				best_y_slot_pos = y;

				if (x==max_column_to_try)
					last_column_taken = true;

				if (y==max_row_to_try)
					last_row_taken = true;

			}

		}
	}

	QPoint pos = posFromSlot(best_x_slot_pos, best_y_slot_pos);
	x_pos = pos.x();
	y_pos = pos.y();
	return true;

}
