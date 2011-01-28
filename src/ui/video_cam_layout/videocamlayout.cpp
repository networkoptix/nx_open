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
#include "ui/videoitem/layout_item.h"
#include "layout_manager.h"
#include "ui/videoitem/video_wnd_archive_item.h"

const int  SLOT_WIDTH = 640*10;
const int  SLOT_HEIGHT = SLOT_WIDTH*3/4;

int SCENE_LEFT = (400*1000);
int SCENE_TOP  = (400*1000);


static const int max_items = 40;

int MAX_FPS_normal = 35;
int MAX_FPS_selected = 60;

extern int scene_zoom_duration;


SceneLayout::SceneLayout():
m_view(0),
m_scene(0),
m_firstTime(true),
m_isRunning(false),
m_editable(false)
{
	connect(&m_timer, SIGNAL(timeout()), this, SLOT(onTimer()));

	connect(&m_videotimer, SIGNAL(timeout()), this, SLOT(onVideoTimer()));
	

	CLGridSettings settings;

	settings.item_distance = 35.0/100;
	settings.items = &m_items;
	settings.left = SCENE_LEFT;
	settings.top = SCENE_TOP;
	settings.max_items = 36;
	settings.max_rows = 5;
	settings.slot_width = SLOT_WIDTH;
	settings.slot_height= SLOT_HEIGHT;

	m_grid.setSettings(settings);
	

	
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

CLGridEngine& SceneLayout::getGridEngine()
{
	return m_grid;
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


void SceneLayout::setMaxFps(int max_fps)
{
	CL_LOG(cl_logDEBUG1)
	{
		cl_log.log("max fps = ", max_fps, cl_logDEBUG1);
	}
	
	m_videotimer.setInterval(1000/max_fps);
}


void SceneLayout::start()
{
	if (m_isRunning)
		return;

	m_firstTime = true;

	m_contentchanged = false;
	m_timer.start(100);
	m_videotimer.start(1000/MAX_FPS_normal); 
	m_isRunning = true;


}

void SceneLayout::stop_helper(bool emt)
{

	m_view->stopAnimation();

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

		CLDevice* dev = devitem->getDevice();
		CLAbstractSceneItem* item = devitem->getSceneItem();

		m_scene->removeItem(item);

		delete devitem; // here video item and dev might be used 
		m_items.removeOne(item);

		delete item;
		dev->releaseRef();

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
	bool added = false;


	
	
	if (m_content->getDeviceCriteria().getCriteria() != CLDeviceCriteria::NONE)
	{
			QList<CLAbstractComplicatedItem*> remove_lst;

			foreach(CLAbstractComplicatedItem* devitem, m_deviceitems)
			{
				if (devitem->getDevice()->getDeviceType() == CLDevice::VIDEODEVICE &&
					!devitem->getDevice()->checkDeviceTypeFlag(CLDevice::RECORDED)) // if this is video device
				{
					bool need_to_remove = true;
					foreach(CLDevice* dev, all_devs)
					{
						if (devitem->getDevice()->getUniqueId() == dev->getUniqueId())
						{
							need_to_remove = false;
							break;
						}
					}

					if (need_to_remove)
						remove_lst.push_back(devitem);
				}
			}
			
			
			if (removeDevices(remove_lst))
				added = true;

			if (m_deviceitems.count()==0)	
				CLGLRenderer::clearGarbage();

	}


	foreach(CLDevice* dev, all_devs)
	{
		// the ref counter for device already increased in getDeviceList; 
		// must not do it again
		if (!addDevice(dev, false))
			dev->releaseRef();
		else
			added = true;
	}


	//==============recorders ================================
	QList<LayoutContent*> children_lst =  m_content->childrenList();
	foreach(LayoutContent* child, children_lst)
	{
		// form list of recorders

		if(!child->isRecorder())
			continue;

		if (addDevice(child->getName(), false))
			added = true;

	}


	//================================

	if (added && !m_firstTime)
	{
		m_view->instantArrange();
		
		updateSceneRect();

		m_view->zoomMin(0);

		m_view->centerOn(m_view->getRealSceneRect().center());
		m_view->fitInView(2000, 0);

		
	}

	
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

bool SceneLayout::addDevice(QString uniqueid, bool update_scene_rect)
{
	//Mind Vibes - mixed by Kick Bong
	//Go To Bed World (Vol. 2) - mixed by Xerxes
	//The Mind Expander - mixed by DJ River & Espe

	CLDevice* dev = CLDeviceManager::instance().getDeviceById(uniqueid);
	if (dev==0)
	{
		dev = CLDeviceManager::instance().getRecorderById(uniqueid);

		if (dev==0)
		{
			dev = CLDeviceManager::instance().getArchiveDevice(uniqueid);

			if (dev==0)
				return false;
		}

	}

	if (!addDevice(dev, update_scene_rect))
	{
		dev->releaseRef();
		return false;
	}


	return true;
}

bool SceneLayout::addDevice(CLDevice* device, bool update_scene_rect)
{
	if (!m_grid.isSpaceAvalable())
	{
		cl_log.log("Cannot support so many devices ", cl_logWARNING);
		return false;
	}

	foreach(CLAbstractComplicatedItem* devitem, m_deviceitems)
	{
		if (devitem->getDevice()->getUniqueId() == device->getUniqueId())
			return false; // already have such device here 
	}



	CLDevice::DeviceType type = device->getDeviceType();

	if (type==CLDevice::VIDEODEVICE)
	{
		QSize wnd_size = m_grid.calcDefaultMaxItemSize(device->getVideoLayout());

		CLVideoWindowItem* video_wnd = 0;

		if (device->checkDeviceTypeFlag(CLDevice::ARCHIVE))//&& false) //777
		{
			video_wnd = new CLVideoWindowArchiveItem(m_view, device->getVideoLayout(), wnd_size.width() , wnd_size.height());
			video_wnd->setEditable(true);
		}
		else
			video_wnd = new CLVideoWindowItem(m_view, device->getVideoLayout(), wnd_size.width() , wnd_size.height());

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



bool SceneLayout::addLayoutItem(QString name, LayoutContent* lc, bool update_scene_rect)
{
	// new item should always be adjusted 
	if (!m_grid.isSpaceAvalable())
		return false;

	CLLayoutItem* item = new CLLayoutItem(m_view, SLOT_WIDTH, SLOT_HEIGHT, name, name);
	item->setRefContent(lc);

	addItem(item, update_scene_rect);

	if (update_scene_rect)
	{
		//m_view->centerOn(m_view->getRealSceneRect().center());
		m_view->fitInView(1000, 0);
	}

	return true;
}

bool SceneLayout::addItem(CLAbstractSceneItem* item, int x, int y, bool update_scene_rect)
{
	// new item should always be adjusted 
	if (!m_grid.isSpaceAvalable())
		return false;

	if (!item)
		return false;

	m_items.push_back(item);

	m_scene->addItem(item);
	item->setPos(x, y);

	//=========
	if (isEditable() || item->isEtitable())	
	{
		item->addSubItem(CloseSubItem);
	}
	//=========

	if (update_scene_rect)
		updateSceneRect();

	connect(item, SIGNAL(onAspectRatioChanged(CLAbstractSceneItem*)), this, SLOT(onAspectRatioChanged(CLAbstractSceneItem*)));

	connect(item, SIGNAL(onPressed(CLAbstractSceneItem*)), this, SLOT(onItemPressed(CLAbstractSceneItem*)));

	connect(item, SIGNAL(onDoubleClick(CLAbstractSceneItem*)), this, SLOT(onItemDoubleClick(CLAbstractSceneItem*)));

	connect(item, SIGNAL(onFullScreen(CLAbstractSceneItem*)), this, SLOT(onItemFullScreen(CLAbstractSceneItem*)));

	connect(item, SIGNAL(onSelected(CLAbstractSceneItem*)), this, SLOT(onItemSelected(CLAbstractSceneItem* )));

	//===========
	connect(item, SIGNAL(onClose(CLAbstractSubItemContainer*)), this, SLOT(onItemClose(CLAbstractSubItemContainer*)));
	

	return true;
}

bool SceneLayout::addItem(CLAbstractSceneItem* item, bool update_scene_rect )
{
	int x, y;

	if (!m_grid.getNextAvailablePos(item->getMaxSize(), x, y))
		return false;

	addItem(item, x, y, update_scene_rect);
	item->setArranged(true);
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
	{
		updateSceneRect();
		m_view->fitInView(600, 0, CLAnimationTimeLine::SLOW_START_SLOW_END);
	}


}


void SceneLayout::setItemDistance(qreal distance)
{
	m_grid.getSettings().item_distance = distance;

	foreach (CLAbstractSceneItem* item, m_items)
	{
		item->setMaxSize(m_grid.getItemMaxSize(item));
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

void SceneLayout::updateSceneRect()
{
	QRect r = m_grid.getGridRect();
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


//===============================================================
void SceneLayout::onItemClose(CLAbstractSubItemContainer* itm)
{

	CLAbstractSceneItem* item = static_cast<CLAbstractSceneItem*>(itm);
	

	m_view->stopAnimation();
	m_view->setZeroSelection();
	item->stop_animation();


	

	CLAbstractSceneItem::CLSceneItemType type = item->getType();
	if (type==CLAbstractSceneItem::LAYOUT)
	{
		CLLayoutItem* litem = static_cast<CLLayoutItem*>(item);
		LayoutContent* lc = litem->getRefContent();
		getContent()->removeLayout(lc, true);
	}

	if (type==CLAbstractSceneItem::RECORDER)
	{
		CLRecorderItem* ritem = static_cast<CLRecorderItem*>(item);
		LayoutContent* lc = ritem->getRefContent();
		getContent()->removeLayout(lc, true);
	}

	if (type==CLAbstractSceneItem::VIDEO)
	{
		CLVideoWindowItem* vitem = static_cast<CLVideoWindowItem*>(item);
		CLAbstractComplicatedItem* devitem = vitem->getVideoCam();
		getContent()->removeDevice(vitem->getComplicatedItem()->getDevice()->getUniqueId());
	}
	//===============


	if (item->getComplicatedItem())
	{
		CLAbstractComplicatedItem* devitem = item->getComplicatedItem();

		devitem->beforestopDispay();
		devitem->stopDispay();
		CLDevice* dev =  devitem->getDevice();
		m_deviceitems.removeOne(devitem);
		removeItem(item); // here device is used. ( bounding rect; layout );
		delete devitem; // here dev and item might be used; can not delete item
		delete item;

		// so need to release it here 
		dev->releaseRef();
	}
	else
	{
		//simple item 
		removeItem(item);
		delete item;
	}


}

bool SceneLayout::removeDevices(QList<CLAbstractComplicatedItem*> lst)
{

	if (lst.count()==0)
		return false;

	foreach(CLAbstractComplicatedItem* devitem, lst)
	{
		// firs of all lets ask all threads to stop;
		// we do not need to do it sequentially
		devitem->beforestopDispay();
	}

	foreach(CLAbstractComplicatedItem* devitem, lst)
	{
		onItemClose(devitem->getSceneItem());
	}
	
	return true;
}

void SceneLayout::onItemSelected(CLAbstractSceneItem* item)
{
	if (item->getType() == CLAbstractSceneItem::RECORDER || item->getType() == CLAbstractSceneItem::LAYOUT)
	{
		CLRecorderItem* ritem = static_cast<CLRecorderItem*>(item);
		emit onNewLayoutItemSelected(ritem->getRefContent());
	}
}

void SceneLayout::onItemFullScreen(CLAbstractSceneItem* item)
{
	if (item->getType() == CLAbstractSceneItem::RECORDER || item->getType() == CLAbstractSceneItem::LAYOUT)
	{
		CLRecorderItem* ritem = static_cast<CLRecorderItem*>(item);
		emit onNewLayoutSelected(m_content, ritem->getRefContent());
	}
}

void SceneLayout::onItemDoubleClick(CLAbstractSceneItem* item)
{
	if (item->getType() == CLAbstractSceneItem::RECORDER || item->getType() == CLAbstractSceneItem::LAYOUT)
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
	if (!hasSuchItem(item))
		return;

	if (item->isArranged())
		m_grid.adjustItem(item);
}

//===============================================================

void SceneLayout::loadContent()
{
	QList<LayoutImage*>& img_list = m_content->getImages();
	QList<LayoutButton*>& btns_list = m_content->getButtons();
	QList<LayoutContent*>& children_list = m_content->childrenList();
	QList<LayoutDevice*>& devices_list = m_content->getDevices();

	bool added = false;

	foreach(LayoutImage* img, img_list)
	{
		CLStaticImageItem* item = new CLStaticImageItem(m_view, img->width(), img->height(), img->getImage(), img->getName());
		item->setOpacity(0.8);
		addItem(item, img->getX(), img->getY());
		added = true;
	}

	foreach(LayoutButton* btn, btns_list)
	{
		CLCustomBtnItem* item = new CLCustomBtnItem(m_view, btn->width(), btn->height(), btn->getName(), btn->getName(), "tiiktip text");
		addItem(item, btn->getX(), btn->getY());
		added = true;
	}

	foreach(LayoutContent* children, children_list)
	{
		if (!children->isRecorder())
		{
			addLayoutItem(children->getName(), children, false);
			added = true;
		}
	}

	foreach(LayoutDevice* dev, devices_list)
	{
		addDevice(dev->getId(), false);
		added = true;
	}

	if (added)
		updateSceneRect();

}

void SceneLayout::saveContent()
{

}

