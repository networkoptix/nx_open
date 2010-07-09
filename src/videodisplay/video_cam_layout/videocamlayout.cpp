#include "./videocamlayout.h"
#include "../../camera/camera.h"
#include "../../device/device_video_layout.h"
#include "../../device/device.h"

#include <QPointF>
#include "graphicsview.h"
#include <QGraphicsScene>

#define SLOT_WIDTH 640*10
#define SLOT_HEIGHT (SLOT_WIDTH*3/4)

#define SCENE_LEFT (200*1000)
#define SCENE_TOP (200*1000)

static const int max_items = 256;



VideoCamerasLayout::VideoCamerasLayout(GraphicsView* view, QGraphicsScene* scene, unsigned int max_rows, int item_distance):
m_view(view),
m_scene(scene),
m_height(max_rows),
m_item_distance(item_distance/100.0),
m_max_items(max_items),
m_slots(max_items*4)
{
	m_width = m_slots/m_height;
	buildPotantial();
}

VideoCamerasLayout::~VideoCamerasLayout()
{
	delete[] m_potantial_x;
	delete[] m_potantial_y;

}

QSize VideoCamerasLayout::getMaxWndSize(const CLDeviceVideoLayout* layout) const
{
	int dlw = layout->width();
	int dlh = layout->height();

	return QSize(SLOT_WIDTH*(dlw + m_item_distance*(dlw-1)), SLOT_HEIGHT*(dlh + m_item_distance*(dlh-1)));
}

QRect VideoCamerasLayout::getLayoutRect() const
{
	CLVideoWindow* wnd =  getVeryLeftWnd();

	if (!wnd) // nos single video on this lay out
		return QRect(SCENE_LEFT, SCENE_TOP, 1, 1 );
	int left = wnd->mapToScene(wnd->boundingRect().topLeft()).x();

	wnd =  getVeryTopWnd();
	int top = wnd->mapToScene(wnd->boundingRect().topLeft()).y();

	wnd =  getVeryRightWnd();
	int right = wnd->mapToScene(wnd->boundingRect().bottomRight()).x();

	wnd =  getVeryBottomWnd();
	int bottom = wnd->mapToScene(wnd->boundingRect().bottomRight()).y();

	QRect video_rect(QPoint(left, top), QPoint(right, bottom) );

	return video_rect;
}

bool VideoCamerasLayout::getNextAvailablePos(const CLDeviceVideoLayout* layout, int &x, int &y) const
{
	// new item should always be adjusted 
	if (!isSpaceAvalable())
		return false;

	//QPoint mass_cnt = getMassCenter();
	QPoint mass_cnt = QPoint(SCENE_LEFT,SCENE_TOP); 

	int center_slot = slotFromPos(mass_cnt);
	int center_slot_x = center_slot%m_width;
	int center_slot_y = center_slot/m_width;

	for (int i = 0; i < m_total_potential_elemnts; ++i)
	{
		int candidate_x = center_slot_x + m_potantial_x[i];
		int candidate_y = center_slot_y + m_potantial_y[i];

		if (candidate_x < 0 || candidate_x > m_width-1 || candidate_y < 0 ||  candidate_y > m_height-1) 
			continue;
		
		int slot_cand = candidate_y*m_width + candidate_x;

		if (isSlotAvailable(slot_cand, layout))
		{
			x = posFromSlot(slot_cand).x();
			y = posFromSlot(slot_cand).y();

			return true;
		}
	}

	return false;

}

bool VideoCamerasLayout::isSpaceAvalable() const
{
	return (m_wnds.size()<m_max_items);
}

bool VideoCamerasLayout::addWnd(CLVideoWindow* wnd, int x, int y, int z_order, bool update_scene_rect)
{
	// new item should always be adjusted 
	if (!isSpaceAvalable())
		return false;

	if (!wnd)
		return false;

	m_wnds.insert(wnd);

	m_scene->addItem(wnd);
	wnd->setPos(x, y);

	//=========

	if (update_scene_rect)
		updateSceneRect();

	connect(wnd, SIGNAL(onAspectRatioChanged(CLVideoWindow*)), this, SLOT(onAspectRatioChanged(CLVideoWindow*)));

}

bool VideoCamerasLayout::addWnd(CLVideoWindow* wnd,  int z_order, bool update_scene_rect )
{
	int x, y;

	if (!getNextAvailablePos(wnd->getVideoLayout(), x, y))
		return false;

	addWnd(wnd, x, y, z_order, update_scene_rect);
	wnd->setArranged(true);

}


// remove wnd from lay out
void VideoCamerasLayout::removeWnd(CLVideoWindow* wnd, bool update_scene_rect )
{
	if (!wnd)
		return;

	m_wnds.remove(wnd);
	m_scene->removeItem(wnd);

	if (update_scene_rect)
		updateSceneRect();


}

void VideoCamerasLayout::updateSceneRect()
{
	QRect r = getLayoutRect();
	m_view->setSceneRect(0,0, 2*r.right(), 2*r.bottom());
	m_view->setRealSceneRect(r);

}

QSet<CLVideoWindow*> VideoCamerasLayout::getWndList() const
{
	return m_wnds;
}



bool VideoCamerasLayout::hasSuchWnd(const CLVideoWindow* wnd) const
{
	CLVideoWindow* wnd_t = const_cast<CLVideoWindow*>(wnd);
	return m_wnds.contains(wnd_t);
}

bool VideoCamerasLayout::hasSuchCam(const CLVideoCamera* cam) const
{
	const CLVideoWindow* wnd = cam->getVideoWindow();
	CLVideoWindow* wnd_t = const_cast<CLVideoWindow*>(wnd);

	return m_wnds.contains(wnd_t);
}


// return wnd on the center of the lay out;
// returns 0 if there is no wnd at all
CLVideoWindow*  VideoCamerasLayout::getCenterWnd() const
{
	QPoint mass_cnt = getMassCenter();

	unsigned int min_distance = 0xffffffff;

	CLVideoWindow* result = 0;

	foreach (CLVideoWindow* wnd, m_wnds)
	{
		QPointF p = wnd->mapToScene(wnd->boundingRect().center());
		int dx = mass_cnt.x() - p.x();
		int dy = mass_cnt.y() - p.y();
		
		unsigned int d = dx*dx + dy*dy;

		if (d < min_distance)
		{
			min_distance = d;
			result = wnd;
		}

	}

	return result;
}

//===============================================================
int VideoCamerasLayout::next_wnd_helper_get_quarter(const QPointF& current, const QPointF& other) const
{

	/*

	\	/
   	 \ /
	  *
	 / \
	/	\

	up(0)
	right(1)
	left(3)
	down(2)

	/**/


	int dx = current.x() - other.x();
	int dy = current.y() - other.y();
	
	if (dx*dx + dy*dy<8)
		return -1; // if distance is so small we should

	qreal dir;

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

CLVideoWindow* VideoCamerasLayout::next_wnd_helper(const CLVideoWindow* curr, int dir_c, int dir_f) const
{
	if (!curr)
		return 0;

	if (m_wnds.size()==1)
		return const_cast<CLVideoWindow*>(curr);


	QPointF cP = curr->mapToScene(curr->boundingRect().center());

	//looking for closest left wnd 
	int dx, dy, distance, min_distance = 0xffffff, max_distance = 0;

	CLVideoWindow* result = 0;

	foreach (CLVideoWindow* wnd, m_wnds)
	{
		QPointF p = wnd->mapToScene(wnd->boundingRect().center());

		if (next_wnd_helper_get_quarter(cP, p)!=dir_c)
			continue;


		dx = cP.x() - p.x();
		dy = cP.y() - p.y();

		distance = dx*dx + dy*dy;

		if (distance < min_distance)
		{
			min_distance = distance;
			result = wnd;
		}
	}

	if (result)
		return result;

	// need to find very right wnd

	foreach (CLVideoWindow* wnd, m_wnds)
	{
		QPointF p = wnd->mapToScene(wnd->boundingRect().center());

		if (next_wnd_helper_get_quarter(cP, p)!=dir_f)
			continue;

		dx = cP.x() - p.x();
		dy = cP.y() - p.y();

		distance = dx*dx + dy*dy;

		if (distance > max_distance)
		{
			max_distance = distance;
			result = wnd;
		}
	}

	if (result)
		return result;

	return const_cast<CLVideoWindow*>(curr);

}

void VideoCamerasLayout::onAspectRatioChanged(CLVideoWindow* wnd)
{
	if (wnd->isArranged())
		adjustWnd(wnd);
}

CLVideoWindow* VideoCamerasLayout::getNextLeftWnd(const CLVideoWindow* curr) const
{
	return next_wnd_helper(curr, 3, 1);
}

CLVideoWindow* VideoCamerasLayout::getNextRightWnd(const CLVideoWindow* curr) const
{
	return next_wnd_helper(curr, 1, 3);
}

CLVideoWindow* VideoCamerasLayout::getNextTopWnd(const CLVideoWindow* curr) const
{
	return next_wnd_helper(curr, 0, 2);
}

CLVideoWindow* VideoCamerasLayout::getNextBottomWnd(const CLVideoWindow* curr) const
{
	return next_wnd_helper(curr, 2, 0);
}

//===============================================================
bool VideoCamerasLayout::isSlotAvailable_advance_helper(int slot, const CLVideoWindow* curr, QList<CLIdealWndPos>& lst) const
{

	// we need to ignore wnds from lst and current one 

	QPoint slPos = posFromSlot(slot);
	QRectF new_wnd_rect(slPos, QSize(curr->width(), curr->height()));

	foreach (CLVideoWindow* wnd, m_wnds)
	{
		if (wnd == curr)
			continue;

		bool need_continue = false;
		for (int it = 0; it < lst.size(); ++it)
		{
			CLIdealWndPos pos = lst.at(it);
			
			if (pos.wnd==wnd)
			{
				// this wnd will be in diff pos
				need_continue = true;
				break;
			}
		}

		if (need_continue)
			continue;


		QRectF wnd_rect = wnd->mapToScene(wnd->boundingRect()).boundingRect();

		if (new_wnd_rect.intersects(wnd_rect))
			return false;		
	}

	return true;

}

QPoint VideoCamerasLayout::getNextCloserstAvailableForWndSlot_butFrom_list___helper(const CLVideoWindow* wnd, QList<CLIdealWndPos>& lst) const
{

	// this function search for next available nearest slot for this wnd
	// if slot already in lst; we ignore such slot 
	QPointF pf = wnd->mapToScene(wnd->boundingRect().topLeft());

	int center_slot = slotFromPos(QPoint(pf.x(), pf.y()));
	int center_slot_x = center_slot%m_width;
	int center_slot_y = center_slot/m_width;

	for (int i = 0; i < m_total_potential_elemnts; ++i)
	{
		int candidate_x = center_slot_x + m_potantial_x[i];
		int candidate_y = center_slot_y + m_potantial_x[i];

		if (candidate_x < 0 || candidate_x > m_width-1 || candidate_y < 0 ||  candidate_y > m_height-1) 
			continue;

		int slot_cand = candidate_y*m_width + candidate_x;

		bool need_continue = false;
		for (int it = 0; it < lst.size(); ++it)
		{
			CLIdealWndPos pos = lst.at(it);
			int lst_slot = slotFromPos(pos.pos);
			if (lst_slot==slot_cand)
			{
				// this slot is already taken 
				need_continue = true;
				break;
			}
		}

		if (need_continue)
			continue;

		// check if slot is available;
		// we need to ignore wnds from lst and current one 
		if (isSlotAvailable_advance_helper(slot_cand, wnd, lst))
			return posFromSlot(slot_cand);

	}


	return getMassCenter();
}

QList<CLIdealWndPos> VideoCamerasLayout::calcArrangedPos() const
{
	QList<CLIdealWndPos> result;

	foreach (CLVideoWindow* wnd, m_wnds)
	{
		CLIdealWndPos pos;
		pos.wnd = wnd;
		pos.pos = getNextCloserstAvailableForWndSlot_butFrom_list___helper(wnd, result);

		int width = wnd->width();
		int height = wnd->height();

		QSize max_size = getMaxWndSize(wnd->getVideoLayout());

		pos.pos.rx() += (max_size.width() - width)/2;
		pos.pos.ry() += (max_size.height() - height)/2;

		


		result.push_back(pos);
	}

	return result;
}


//===============================================================
void VideoCamerasLayout::adjustWnd(CLVideoWindow* wnd) const
{
	QPointF p = wnd->mapToScene(wnd->boundingRect().topLeft());

	QPoint new_p  = posFromSlot(slotFromPos( QPoint(p.x(),p.y()) ));

	int width = wnd->width();
	int height = wnd->height();

	QSize max_size = getMaxWndSize(wnd->getVideoLayout());

	new_p.rx() += (max_size.width() - width)/2;
	new_p.ry() += (max_size.height() - height)/2;

	wnd->setPos(new_p);

}

//===============================================================
CLVideoWindow* VideoCamerasLayout::getVeryLeftWnd() const
{
	CLVideoWindow* result = 0;

	unsigned int min_val = 0xffffffff; // very big value

	foreach (CLVideoWindow* wnd, m_wnds)
	{
		QPointF p = wnd->mapToScene(wnd->boundingRect().topLeft());
		if (p.x() < min_val)
		{
			min_val = p.x();
			result = wnd;
		}
	}

	return result;
}

CLVideoWindow* VideoCamerasLayout::getVeryTopWnd() const
{
	CLVideoWindow* result = 0;

	unsigned int min_val = 0xffffffff; // very big value

	foreach (CLVideoWindow* wnd, m_wnds)
	{
		QPointF p = wnd->mapToScene(wnd->boundingRect().topLeft());
		if (p.y() < min_val)
		{
			min_val = p.y();
			result = wnd;
		}
	}

	return result;
}



CLVideoWindow* VideoCamerasLayout::getVeryRightWnd() const
{
	CLVideoWindow* result = 0;

	unsigned int max_val = 0;

	foreach (CLVideoWindow* wnd, m_wnds)
	{
		QPointF p = wnd->mapToScene(wnd->boundingRect().bottomRight());
		if (p.x() >= max_val)
		{
			max_val = p.x();
			result = wnd;
		}
	}

	return result;
}

CLVideoWindow* VideoCamerasLayout::getVeryBottomWnd() const
{
	CLVideoWindow* result = 0;

	unsigned int max_val = 0;

	foreach (CLVideoWindow* wnd, m_wnds)
	{
		QPointF p  = wnd->mapToScene(wnd->boundingRect().bottomRight());
		if (p.y() >= max_val)
		{
			max_val = p.y();
			result = wnd;
		}
	}

	return result;

}

QPoint VideoCamerasLayout::getMassCenter() const
{
	if (m_wnds.size()==0)
		return QPoint(SCENE_LEFT, SCENE_TOP);

	QPointF result(0.0,0.0);

	foreach (CLVideoWindow* wnd, m_wnds)
	{
		QPointF p  = wnd->mapToScene(wnd->boundingRect().center());
		result+=p;
	}

	return QPoint(result.x()/m_wnds.size(), result.y()/m_wnds.size());


}

int VideoCamerasLayout::slotFromPos(QPoint p) const
{
	int x = p.x() - SCENE_LEFT;
	int y = p.y() - SCENE_TOP;

	x = x/( SLOT_WIDTH*(1+m_item_distance) );
	y = y/( SLOT_HEIGHT*(1+m_item_distance) );

	return y*m_width + x;
}

QPoint VideoCamerasLayout::posFromSlot(int slot) const
{
	int x = slot%m_width;
	int y = slot/m_width;

	return QPoint(SCENE_LEFT + x*SLOT_WIDTH*(1+m_item_distance), SCENE_TOP + y*SLOT_HEIGHT*(1+m_item_distance));
}

bool VideoCamerasLayout::isSlotAvailable(int slot, QSize size) const
{
	QPoint slPos = posFromSlot(slot);
	QRectF new_wnd_rect(slPos, size);

	foreach (CLVideoWindow* wnd, m_wnds)
	{
		QRectF wnd_rect = wnd->mapToScene(wnd->boundingRect()).boundingRect();

		if (new_wnd_rect.intersects(wnd_rect))
			return false;		
	}

	return true;

}

bool VideoCamerasLayout::isSlotAvailable(int slot, const CLDeviceVideoLayout* layout) const
{
	QSize size = getMaxWndSize(layout);
	return isSlotAvailable(slot, size);
}

int VideoCamerasLayout::bPh_findNext(int *energy, int elemnts)
{
	//need to find an element with minimum energy 

	unsigned int min_e = 0xffffffff; // big value

	int result = 0;

	for (int i = 0; i < elemnts; ++i)
	{
		if (energy[i]<min_e)
		{
			result = i;
			min_e = energy[i];
		}
	}

	energy[result] = 0xfffffff; // big val; do no take it next time

	return result;
}

void VideoCamerasLayout::buildPotantial()
{
	//if we have m_slots => maximum number of spiral elements is 5*m_slots( may be a bit less )

	m_total_potential_elemnts = (2*m_width)*(2*m_height)+1;

	m_potantial_x = new int[m_total_potential_elemnts];
	m_potantial_y = new int[m_total_potential_elemnts];

	int *potential_energy = new int[m_total_potential_elemnts];

	int center_x = m_width;
	int center_y = m_height;

	for (int i = 0; i < m_total_potential_elemnts; ++i)
	{
		int curr_x = i%(2*m_width);
		int curr_y = i/(2*m_width);

		int dx = abs(curr_x - center_x);
		int dy = abs(curr_y - center_y);

		potential_energy[i] = 4*dx*dx + 4*dy*dy*4/3;
	}

	for (int i = 0; i < m_total_potential_elemnts; ++i)
	{
		int pos = bPh_findNext(potential_energy, m_total_potential_elemnts);

		int curr_x = pos%(2*m_width);
		int curr_y = pos/(2*m_width);

		m_potantial_x[i] = center_x - curr_x;
		m_potantial_y[i] = center_y - curr_y;

	}

	delete[] potential_energy;

	
}

