#include "layout_content.h"


LayoutContent::LayoutContent():
m_cr(CLDeviceCriteria::NONE),
mDecoration(0),
m_recorder(false),
m_editable(false)
{
	setName("Layout:unnamed");
}

LayoutContent::~LayoutContent()
{
	foreach (LayoutImage* item, m_imgs)
	{
		delete item;
	}

	foreach (LayoutButton* item, m_btns)
	{
		delete item;
	}

	foreach (LayoutDevice* item, m_devices)
	{
		delete item;
	}

	foreach (LayoutContent* cont, m_childlist)
	{
		delete cont;
	}

}

LayoutItem::Type LayoutContent::type() const
{
	return LAYOUT;
}

bool LayoutContent::isRecorder() const
{
	return m_recorder;
}

void LayoutContent::setRecorder()
{
	m_recorder = true;
}

bool LayoutContent::isEditable() const
{
	return m_editable;
}

void LayoutContent::setEditable(bool editable) 
{
	m_editable = editable;
}

bool LayoutContent::hasSuchSublayoutName(const QString& name) const
{
	foreach (LayoutContent* cont, m_childlist)
	{
		if (cont->getName()==name)
			return true;
	}

	return false;
}


bool LayoutContent::checkDecorationFlag(unsigned int flag) const
{
	return mDecoration & flag;
}

void LayoutContent::addDecorationFlag(unsigned int flag)
{
	mDecoration |= flag;
}


void LayoutContent::setParent(LayoutContent* parent)
{
	m_parent = parent;
}

LayoutContent* LayoutContent::getParent() const
{
	return m_parent;
}

void LayoutContent::addButton(const QString& name, const QString& text, const QString& tooltip, int x, int y, int width, int height, int angle)
{
	m_btns.push_back(new LayoutButton(name, text, tooltip, x, y, width, height, angle) );
}

void LayoutContent::addImage(const QString& img, const QString& name, const QString& text, const QString& tooltip, int x, int y, int width, int height, int angle)
{
	m_imgs.push_back(new LayoutImage(img, name, text, tooltip, x, y, width, height, angle));
}

void LayoutContent::addDevice(const QString& uniqueId, int x, int y, int width, int height, int angle )
{
	m_devices.push_back(new LayoutDevice(uniqueId, x, y, width, height, angle));
}

void LayoutContent::addLayout(LayoutContent* l, bool copy)
{
	if (copy)
		m_childlist.push_back(coppyLayout(l));
	else
		m_childlist.push_back(l);

	m_childlist.back()->setParent(this);

}


void LayoutContent::setDeviceCriteria(const CLDeviceCriteria& cr)
{
	m_cr = cr;
}

CLDeviceCriteria LayoutContent::getDeviceCriteria() const
{
	return m_cr;
}


QList<LayoutImage*>& LayoutContent::getImages() 
{
	return m_imgs;
}

QList<LayoutButton*>& LayoutContent::getButtons() 
{
	return m_btns;
}

QList<LayoutDevice*>& LayoutContent::getDevices()
{
	return m_devices;
}

QList<LayoutContent*>& LayoutContent::childrenList() 
{
	return m_childlist;
}

//=========================================================================

LayoutContent* LayoutContent::coppyLayout(LayoutContent* l)
{
	LayoutContent* result = new LayoutContent();
	*result = *l; // this copy everything including lists of pointers; we do not need that;

	m_imgs.clear();
	m_btns.clear();
	m_devices.clear();
	m_childlist.clear();

	foreach (LayoutImage* item, l->m_imgs)
	{
		LayoutImage* iteml = new LayoutImage();
		*iteml = *item;
		m_imgs.push_back(iteml);
	}

	foreach (LayoutButton* item, l->m_btns)
	{
		LayoutButton* iteml = new LayoutButton();
		*iteml = *item;
		m_btns.push_back(iteml);
	}

	foreach (LayoutDevice* item, l->m_devices)
	{
		LayoutDevice* iteml = new LayoutDevice();
		*iteml = *item;
		m_devices.push_back(iteml);
	}


	foreach (LayoutContent* cont, l->m_childlist)
	{
		LayoutContent* contl = coppyLayout(cont);
		m_childlist.push_back(contl);
	}


	return result;
}