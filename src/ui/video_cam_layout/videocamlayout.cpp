#include "./videocamlayout.h"
#include "../../camera/camera.h"
#include "../../device/device_video_layout.h"
#include "../../device/device.h"

#include <QPointF>
#include "./../graphicsview.h"
#include <QGraphicsScene>
#include "ui/videoitem/video_wnd_item.h"
#include "settings.h"
#include "device/device_managmen/device_manager.h"
#include "video_camera.h"
#include "base/log.h"
#include "ui/videoitem/static_image_item.h"
#include "ui/videoitem/custom_draw_button.h"
#include "layout_content.h"
#include "recorder/recorder_display.h"
#include "ui/videoitem/recorder_item.h"

#define SLOT_WIDTH (640*10)
#define SLOT_HEIGHT (SLOT_WIDTH*3/4)

int SCENE_LEFT = (400*1000);
int SCENE_TOP  = (400*1000);


static const int max_items = 256;
#define MAX_FPS (35.0)
extern int scene_zoom_duration;


bool SceneLayout::m_potantial_builded = false;
int* SceneLayout::m_potantial_x = 0;
int* SceneLayout::m_potantial_y = 0;
int SceneLayout::m_total_potential_elemnts = 0;


SceneLayout::SceneLayout():
m_view(0),
m_scene(0),
m_height(4),
m_item_distance(35/100.0),
m_max_items(max_items),
m_slots(max_items*4),
m_firstTime(true),
m_isRunning(false),
m_editable(false)
{
	m_width = m_slots/m_height;

	if (!m_potantial_builded)
	{
		buildPotantial();
		m_potantial_builded = true;
	}
	

	connect(&m_timer, SIGNAL(timeout()), this, SLOT(onTimer()));

	connect(&m_videotimer, SIGNAL(timeout()), this, SLOT(onVideoTimer()));
	


	
}

SceneLayout::~SceneLayout()
{
	stop();

	//delete[] m_potantial_x;
	//delete[] m_potantial_y;

}

void SceneLayout::setContent(LayoutContent* cont)
{
	m_content = cont;
}

LayoutContent* SceneLayout::getContent()
{
	return m_content;
}

bool SceneLayout::isEditable() const
{
	return m_editable && m_content->isEditable();
}

void SceneLayout::setEditable(bool editable)
{
	m_editable = editable;
}

bool SceneLayout::isContentChanged() const
{
	return m_contentchanged;
}

void SceneLayout::setContentChanged(bool changed)
{
	m_contentchanged = changed;
}



void SceneLayout::start()
{
	if (m_isRunning)
		return;

	m_firstTime = true;

	m_contentchanged = false;
	m_timer.start(100);
	m_videotimer.start(1000/MAX_FPS); 
	m_isRunning = true;
}

void SceneLayout::stop_helper(bool emt)
{

	disconnect(m_view, SIGNAL(scneZoomFinished()), this, SLOT(stop_helper()));

	foreach(CLAbstractComplicatedItem* devitem, m_deviceitems)
	{
		// firs of all lets ask all threads to stop;
		// we do not need to do it sequentially
		devitem->beforestopDispay();
	}

	//===============

	foreach(CLAbstractComplicatedItem* devitem, m_deviceitems)
	{
		// after we can wait for each thread to stop
		cl_log.log("About to shutdown device ", (int)devitem ,"\r\n", cl_logDEBUG1);
		devitem->stopDispay();
		devitem->getDevice()->releaseRef();
		delete devitem;
	}

	m_deviceitems.clear();

	//===================

	foreach(CLAbstractSceneItem* item, m_items)
	{
		m_scene->removeItem(item);
		delete item;
	}

	m_items.clear();

	if (emt)
		emit stoped(m_content);

}

void SceneLayout::stop(bool animation)
{

	if (!m_isRunning)
		return;

	m_isRunning = false;

	cl_log.log("SceneLayout::stop......\r\n ", cl_logDEBUG1);

	m_view->stop();
	
	m_timer.stop();
	m_videotimer.stop();

	

	

	if (animation)
	{
		connect(m_view, SIGNAL(scneZoomFinished()), this, SLOT(stop_helper()));
		m_view->zoomMin(400);
	}
	else
	{
		stop_helper(animation);
	}

}


void SceneLayout::onTimer()
{
	if (m_firstTime)
	{
		m_view->zoomMin(0);
		m_view->initDecoration();
		loadContent();
	}

	if (m_firstTime)
	{
		m_firstTime =  false;
		
		//QThread::currentThread()->setPriority(QThread::IdlePriority); // surprised. if gui thread has low priority => things looks smoother 
		QThread::currentThread()->setPriority(QThread::LowPriority); // surprised. if gui thread has low priority => things looks smoother 
		//QThread::currentThread()->setPriority(QThread::HighestPriority); // surprised. if gui thread has low priority => things looks smoother 
		m_view->start();
	}

	if (m_items.count()>0 && m_timer.interval()!=devices_update_interval)
	{
		m_timer.setInterval(devices_update_interval);
	}

	

	//===================video devices=======================
	CLDeviceList all_devs =  CLDeviceManager::instance().getDeviceList(m_content->getDeviceCriteria());
	bool added = addDevices_helper(all_devs);

	//==============recorders ================================
	CLDeviceList recorders;
	QList<LayoutContent*> children_lst =  m_content->childrenList();
	foreach(LayoutContent* child, children_lst)
	{
		// form list of recorders

		if(!child->isRecorder())
			continue;

		CLDevice* dev = CLDeviceManager::instance().getRecorderById(child->getName());

		if (dev==0)
			continue;

		recorders[dev->getUniqueId()] = dev;
	}

	added = addDevices_helper(recorders) || added;



	if (added && !m_firstTime)
	{
		m_view->centerOn(m_view->getRealSceneRect().center());
		m_view->fitInView(2000);
	}


	
	//====================================
}

bool SceneLayout::addDevices_helper(CLDeviceList& lst)
{
	bool added = false;

	foreach(CLDevice* dev, lst)
	{
		bool contains = false;

		foreach(CLAbstractComplicatedItem* devitem, m_deviceitems)
		{
			if (devitem->getDevice()->getUniqueId() == dev->getUniqueId())
			{
				contains = true;
				break;
			}
		}


		if (contains) // if such device already here we do not need it
		{
			dev->releaseRef();
		}
		else
		{
			// the ref counter for device already increased in getDeviceList; 
			// must not do it again
			if (!addDevice(dev, false))
				dev->releaseRef();
			else
				added = true;
		}
	}

	return added;

}

void SceneLayout::onVideoTimer()
{
	foreach(CLAbstractComplicatedItem* devitem, m_deviceitems)
	{
		CLAbstractSceneItem* itm = devitem->getSceneItem();
		if (itm->needUpdate())
		{
			itm->needUpdate(false);
			itm->update();
		}
	}
}

bool SceneLayout::addDevice(CLDevice* device, bool update_scene_rect)
{
	if (!isSpaceAvalable())
	{
		cl_log.log("Cannot support so many devices ", cl_logWARNING);
		return false;
	}


	CLDevice::DeviceType type = device->getDeviceType();

	if (type==CLDevice::VIDEODEVICE)
	{
		QSize wnd_size = getMaxWndSize(device->getVideoLayout());


		CLVideoWindowItem* video_wnd =  new CLVideoWindowItem(m_view, device->getVideoLayout(), wnd_size.width() , wnd_size.height());
		CLVideoCamera* cam = new VideoCamera(device, video_wnd);
		addItem(video_wnd, update_scene_rect);

		m_deviceitems.push_back(cam);
		cam->setQuality(CLStreamreader::CLSLow, true);
		cam->startDispay();

		return true;
	}

	if (type==CLDevice::RECORDER)
	{
		CLRecorderItem* item = new CLRecorderItem(m_view, SLOT_WIDTH, SLOT_HEIGHT, device->getUniqueId(), device->getUniqueId());

		// need to assign reference layout content
		QList<LayoutContent*> children_lst =  m_content->childrenList();
		foreach(LayoutContent* child, children_lst)
		{
			// form list of recorders

			if(!child->isRecorder())
				continue;

			if (child->getName() == device->getUniqueId())
			{
				item->setRefContent(child);
				break;
			}
		}




		CLRecorderDisplay* recd = new CLRecorderDisplay(device, item);
		addItem(item, update_scene_rect);

		m_deviceitems.push_back(recd);
		recd->start();

		return true;
	}

	return false;

}


bool SceneLayout::removeDevice(CLDevice* dev)
{
	return true;
}


void SceneLayout::setItemDistance(qreal distance)
{
	m_item_distance = distance;

	foreach (CLAbstractSceneItem* item, m_items)
	{
		CLVideoWindowItem* videoItem;
		if (videoItem = item->toVideoItem())
		{
			videoItem->setMaxSize(getMaxWndSize (videoItem->getVideoCam()->getDevice()->getVideoLayout()));
		}
		
	}

}

void SceneLayout::setView(GraphicsView* view)
{
	m_view = view;
}

void SceneLayout::setScene(QGraphicsScene* scene)
{
	m_scene = scene;
}


qreal SceneLayout::getItemDistance() const
{
	return m_item_distance;
}

QSize SceneLayout::getMaxWndSize(const CLDeviceVideoLayout* layout) const
{
	qreal dlw = layout->width();
	qreal dlh = layout->height();

	if (dlw == 4 && dlh == 1)
		dlw = 3;

	return QSize(SLOT_WIDTH*(dlw + m_item_distance*(dlw-1)), SLOT_HEIGHT*(dlh + m_item_distance*(dlh-1)));
}

int SceneLayout::slotsW(CLVideoWindowItem* wnd) const
{
	 return ceil( (qreal(wnd->width())/SLOT_WIDTH + m_item_distance)/(1+m_item_distance) - 1e-7);
}

int SceneLayout::slotsH(CLVideoWindowItem* wnd) const
{
	return ceil( (qreal(wnd->height())/SLOT_HEIGHT+ m_item_distance)/(1+m_item_distance) - 1e-7);
}


QRect SceneLayout::getSmallLayoutRect() const // scene rect 
{
	CLAbstractSceneItem* item =  getVeryLeftItem();

	if (!item) // nos single video on this lay out
		return QRect(SCENE_LEFT, SCENE_TOP, 10, 10 );
	int left = item->mapToScene(item->boundingRect().topLeft()).x();

	item =  getVeryTopItem();
	int top = item->mapToScene(item->boundingRect().topLeft()).y();

	item =  getVeryRightItem();
	int right = item->mapToScene(item->boundingRect().bottomRight()).x();

	item =  getVeryBottomItem();
	int bottom = item->mapToScene(item->boundingRect().bottomRight()).y();

	QRect video_rect(QPoint(left, top), QPoint(right, bottom) );

	video_rect.adjust(-m_item_distance*SLOT_WIDTH/2, - m_item_distance*SLOT_HEIGHT/2, m_item_distance*SLOT_WIDTH/2, m_item_distance*SLOT_HEIGHT/2);

	return video_rect;

}

QRect SceneLayout::getLayoutRect() const
{
	QRect video_rect = getSmallLayoutRect();
	//video_rect.adjust(-SLOT_WIDTH, - SLOT_HEIGHT*2, SLOT_WIDTH, SLOT_HEIGHT*2); // issue wuth 8180 rotation?
	return video_rect;
}

bool SceneLayout::getNextAvailablePos(QSize size, int &x, int &y) const
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

		if (isSlotAvailable(slot_cand, size))
		{
			x = posFromSlot(slot_cand).x();
			y = posFromSlot(slot_cand).y();

			return true;
		}
	}

	return false;

}

bool SceneLayout::isSpaceAvalable() const
{
	return (m_items.size()<m_max_items);
}

bool SceneLayout::addItem(CLAbstractSceneItem* item, int x, int y, bool update_scene_rect)
{
	// new item should always be adjusted 
	if (!isSpaceAvalable())
		return false;

	if (!item)
		return false;

	m_items.push_back(item);

	m_scene->addItem(item);
	item->setPos(x, y);

	//=========

	if (update_scene_rect)
		updateSceneRect();

	connect(item, SIGNAL(onAspectRatioChanged(CLAbstractSceneItem*)), this, SLOT(onAspectRatioChanged(CLAbstractSceneItem*)));

	connect(item, SIGNAL(onPressed(CLAbstractSceneItem*)), this, SLOT(onItemPressed(CLAbstractSceneItem*)));

	connect(item, SIGNAL(onDoubleClick(CLAbstractSceneItem*)), this, SLOT(onItemDoubleClick(CLAbstractSceneItem*)));
	
	connect(item, SIGNAL(onFullScreen(CLAbstractSceneItem*)), this, SLOT(onItemFullScreen(CLAbstractSceneItem*)));

	connect(item, SIGNAL(onSelected(CLAbstractSceneItem*)), this, SLOT(onItemSelected(CLAbstractSceneItem* )));

	

	

	return true;

}

bool SceneLayout::addItem(CLAbstractSceneItem* wnd, bool update_scene_rect )
{
	int x, y;

	if (!getNextAvailablePos(wnd->getMaxSize(), x, y))
		return false;

	addItem(wnd, x, y, update_scene_rect);
	wnd->setArranged(true);
	return true;

}


// remove item from lay out
void SceneLayout::removeItem(CLAbstractSceneItem* item, bool update_scene_rect )
{
	if (!item)
		return;

	m_items.removeOne(item);
	m_scene->removeItem(item);

	if (update_scene_rect)
		updateSceneRect();


}

void SceneLayout::updateSceneRect()
{
	QRect r = getLayoutRect();
	m_view->setSceneRect(0,0, 2*r.right(), 2*r.bottom());
	m_view->setRealSceneRect(r);

}

QList<CLAbstractSceneItem*> SceneLayout::getItemList() const
{
	return m_items;
}



bool SceneLayout::hasSuchItem(const CLAbstractSceneItem* item) const
{
	return m_items.contains(const_cast<CLAbstractSceneItem*>(item));
}

void SceneLayout::makeAllItemsSelectable(bool selectable)
{
	foreach(CLAbstractSceneItem* item, m_items)
	{
		item->setFlag(QGraphicsItem::ItemIsSelectable, selectable);
	}
}

// return wnd on the center of the lay out;
// returns 0 if there is no wnd at all
CLAbstractSceneItem*  SceneLayout::getCenterWnd() const
{
	QPoint mass_cnt = getMassCenter();

	unsigned int min_distance = 0xffffffff;

	CLAbstractSceneItem* result = 0;

	foreach (CLAbstractSceneItem* item, m_items)
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

//===============================================================
int SceneLayout::next_item_helper_get_quarter(const QPointF& current, const QPointF& other) const
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


	int dx = other.x() - current.x();
	int dy = other.y() - current.y();
	
	if ((qreal)dx*dx + dy*dy<8)
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

CLAbstractSceneItem* SceneLayout::next_item_helper(const CLAbstractSceneItem* curr, int dir_c, int dir_f) const
{
	if (!curr)
		return 0;

	if (m_items.size()==1)
		return const_cast<CLAbstractSceneItem*>(curr);


	QPointF cP = curr->mapToScene(curr->boundingRect().center());

	//looking for closest left wnd 
	int dx, dy;
	qreal min_distance = 1e+20, max_distance = 0, distance;

	CLAbstractSceneItem* result = 0;

	foreach (CLAbstractSceneItem* item, m_items)
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

	foreach (CLAbstractSceneItem* item, m_items)
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

void SceneLayout::onItemSelected(CLAbstractSceneItem* item)
{
	if (item->getType() == CLAbstractSceneItem::RECORDER)
	{
		CLRecorderItem* ritem = static_cast<CLRecorderItem*>(item);
		emit onNewLayoutItemSelected(ritem->getRefContent());
	}
}

void SceneLayout::onItemFullScreen(CLAbstractSceneItem* item)
{
	if (item->getType() == CLAbstractSceneItem::RECORDER)
	{
		CLRecorderItem* ritem = static_cast<CLRecorderItem*>(item);
		emit onNewLayoutSelected(m_content, ritem->getRefContent());
	}
}

void SceneLayout::onItemDoubleClick(CLAbstractSceneItem* item)
{
	if (item->getType() == CLAbstractSceneItem::RECORDER)
	{
		CLRecorderItem* ritem = static_cast<CLRecorderItem*>(item);
		emit onNewLayoutSelected(m_content, ritem->getRefContent());
	}
	
}

void SceneLayout::onItemPressed(CLAbstractSceneItem* item)
{
	if (item->getType() == CLAbstractSceneItem::IMAGE || item->getType() == CLAbstractSceneItem::BUTTON)
	{
		emit onItemPressed(m_content, item->getName());
	}
}

void SceneLayout::onAspectRatioChanged(CLAbstractSceneItem* item)
{
	if (item->isArranged())
		adjustItem(item);
}

CLAbstractSceneItem* SceneLayout::getNextLeftItem(const CLAbstractSceneItem* curr) const
{
	return next_item_helper(curr, 3, 1);
}

CLAbstractSceneItem* SceneLayout::getNextRightItem(const CLAbstractSceneItem* curr) const
{
	return next_item_helper(curr, 1, 3);
}

CLAbstractSceneItem* SceneLayout::getNextTopItem(const CLAbstractSceneItem* curr) const
{
	return next_item_helper(curr, 0, 2);
}

CLAbstractSceneItem* SceneLayout::getNextBottomItem(const CLAbstractSceneItem* curr) const
{
	return next_item_helper(curr, 2, 0);
}

//===============================================================

QPoint SceneLayout::getNextCloserstAvailableForWndSlot_butFrom_list___helper(const CLVideoWindowItem* wnd, QList<CLIdealWndPos>& lst) const
{

	// this function search for next available nearest slot for this item
	// if slot already in lst; we ignore such slot 
	//QPointF pf = item->mapToScene(item->boundingRect().topLeft());

	//int center_slot = slotFromPos(QPoint(pf.x(), pf.y()));
	int center_slot_x = 0;//center_slot%m_width;
	int center_slot_y = 0;//center_slot/m_width;

	for (int i = 0; i < m_total_potential_elemnts; ++i)
	{
		int candidate_x = center_slot_x + m_potantial_x[i];
		int candidate_y = center_slot_y + m_potantial_y[i];

		if (candidate_x < 0 || candidate_x > m_width-1 || candidate_y < 0 ||  candidate_y > m_height-1) 
			continue;

		int slot_cand = candidate_y*m_width + candidate_x;

		bool need_continue = false;
		for (int it = 0; it < lst.size(); ++it)
		{
			CLIdealWndPos pos = lst.at(it);
			int lst_slot = slotFromPos(pos.pos);

			int sl_w = slotsW(pos.item);
			int sl_h = slotsH(pos.item);


			for (int x = 0; x < sl_w; ++x)
				for (int y = 0; y < sl_h; ++y)
				{
					if ((lst_slot + y*m_width + x) == slot_cand)
					{
						// this slot is already taken 
						need_continue = true;
						break;
					}
				}
		}

		if (need_continue)
			continue;

		// check if slot is available;
		// we need to ignore wnds from lst and current one 
		return posFromSlot(slot_cand);

	}


	return getMassCenter();
}

QList<CLIdealWndPos> SceneLayout::calcArrangedPos() const
{
	QList<CLIdealWndPos> result;

	foreach (CLAbstractSceneItem* item, m_items)
	{

		CLVideoWindowItem* videoItem = 0;
		if (!(videoItem = item->toVideoItem()))
			continue;


		CLIdealWndPos pos;
		pos.item = videoItem;


		pos.pos = getNextCloserstAvailableForWndSlot_butFrom_list___helper(videoItem, result);

		int slot1 = slotFromPos(pos.pos);

		int width = videoItem->width();
		int height = videoItem->height();

		QSize max_size = getMaxWndSize(videoItem->getVideoLayout());

		pos.pos.rx() += (max_size.width() - width)/2;
		pos.pos.ry() += (max_size.height() - height)/2;

		int slot2 = slotFromPos(pos.pos);

		if (slot1!=slot2)
			slot1 = slot1;
		
		slot2 = slotFromPos(pos.pos);
		width = videoItem->width();


		result.push_back(pos);
	}

	return result;
}

void SceneLayout::loadContent()
{
	QList<LayoutImage*> img_list = m_content->getImages();
	QList<LayoutButton*> btns_list = m_content->getButtons();

	foreach(LayoutImage* img, img_list)
	{
		CLStaticImageItem* item = new CLStaticImageItem(m_view, img->width(), img->height(), img->getImage(), img->getName());
		item->setOpacity(0.8);
		addItem(item, img->getX(), img->getY());
	}

	foreach(LayoutButton* btn, btns_list)
	{
		
		CLCustomBtnItem* item = new CLCustomBtnItem(m_view, btn->width(), btn->height(), btn->getName(), btn->getName(), "tiiktip text");
		addItem(item, btn->getX(), btn->getY());
	}


}

void SceneLayout::saveContent()
{

}


//===============================================================
void SceneLayout::adjustItem(CLAbstractSceneItem* item) const
{
	
	QPointF p = item->mapToScene(item->boundingRect().center());
	p-=QPointF(item->width()/2, item->height()/2); // this addition as not good at all; due to item zoom topLeft pos might be shifted to diff slot; case1


	int slot = slotFromPos( QPoint(p.x(),p.y()) );

	if (slot<0)
	{
		Q_ASSERT(false);
		return;
	}


	QPoint new_p  = posFromSlot( slot ); 

	int width = item->width();
	int height = item->height();

	QSize max_size = item->getMaxSize();

	new_p.rx() += (max_size.width() - width)/2;
	new_p.ry() += (max_size.height() - height)/2;

	item->setPos(new_p);

}

//===============================================================
CLAbstractSceneItem* SceneLayout::getVeryLeftItem() const
{
	CLAbstractSceneItem* result = 0;

	unsigned int min_val = 0xffffffff; // very big value

	foreach (CLAbstractSceneItem* item, m_items)
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

CLAbstractSceneItem* SceneLayout::getVeryTopItem() const
{
	CLAbstractSceneItem* result = 0;

	unsigned int min_val = 0xffffffff; // very big value

	foreach (CLAbstractSceneItem* item, m_items)
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



CLAbstractSceneItem* SceneLayout::getVeryRightItem() const
{
	CLAbstractSceneItem* result = 0;

	unsigned int max_val = 0;

	foreach (CLAbstractSceneItem* item, m_items)
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

CLAbstractSceneItem* SceneLayout::getVeryBottomItem() const
{
	CLAbstractSceneItem* result = 0;

	unsigned int max_val = 0;

	foreach (CLAbstractSceneItem* item, m_items)
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

QPoint SceneLayout::getMassCenter() const
{
	if (m_items.size()==0)
		return QPoint(SCENE_LEFT, SCENE_TOP);

	QPointF result(0.0,0.0);

	foreach (CLAbstractSceneItem* item, m_items)
	{
		QPointF p  = item->mapToScene(item->boundingRect().center());
		result+=p;
	}

	return QPoint(result.x()/m_items.size(), result.y()/m_items.size());


}

int SceneLayout::slotFromPos(QPoint p) const
{
	int x = p.x() - SCENE_LEFT;
	int y = p.y() - SCENE_TOP;

	x = x/( SLOT_WIDTH*(1+m_item_distance - 1e-10) );
	y = y/( SLOT_HEIGHT*(1+m_item_distance- 1e-10) );

	return y*m_width + x;
}

QPoint SceneLayout::posFromSlot(int slot) const
{
	int x = slot%m_width;
	int y = slot/m_width;

	return QPoint(SCENE_LEFT + x*SLOT_WIDTH*(1+m_item_distance), SCENE_TOP + y*SLOT_HEIGHT*(1+m_item_distance));
}

bool SceneLayout::isSlotAvailable(int slot, QSize size) const
{
	QPoint slPos = posFromSlot(slot);
	QRectF new_wnd_rect(slPos, size);

	foreach (CLAbstractSceneItem* item, m_items)
	{
		QRectF wnd_rect = item->mapToScene(item->boundingRect()).boundingRect();

		if (new_wnd_rect.intersects(wnd_rect))
			return false;		
	}

	return true;

}


int SceneLayout::bPh_findNext(int *energy, int elemnts)
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

void SceneLayout::buildPotantial()
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

		potential_energy[i] = 4*dx*dx + 4*dy*dy*(10.0/7);
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

