#include "layout_content.h"


LayoutItem::LayoutItem()
{

}

LayoutItem::LayoutItem(int x_, int y_, int width_, int height_, int angle_):
x(x_),
y(y_),
w(width_),
h(height_),
angl(angle_)
{

}


LayoutItem::~LayoutItem()
{

}

void LayoutItem::setName(const QString& name)
{
	this->name = name;
}

QString LayoutItem::getName() const
{
	return name;
}



int LayoutItem::getX() const
{
	return x;
}


int LayoutItem::getY() const
{
	return y;
}

QPointF LayoutItem::pos() const
{
	return QPointF(x,y);
}


int LayoutItem::width() const
{
	return w;
}

int LayoutItem::height() const
{
	return h;
}

QSize LayoutItem::size() const
{
	return QSize(x,y);
}

int LayoutItem::angle() const
{
	return angl;
}

//=======

LayoutDevice::LayoutDevice(const QString& uniqueId, int x_, int y_, int width_, int height_, int angle_):
LayoutItem(x_, y_, width_, height_,  angle_),
id(uniqueId)
{

}

LayoutItem::Type LayoutDevice::type() const
{
	return DEVICE;
}

QString LayoutDevice::getId() const
{
	return id;
}

//=======
LayoutButton::LayoutButton()
{

}

LayoutButton::LayoutButton(const QString& name_, const QString& text, const QString& tooltip, int x_, int y_, int width_, int height_, int angle_):
LayoutItem(x_, y_, width_, height_,  angle_),
m_text(text),
m_tooltip(tooltip)
{
	setName(name_);
}

LayoutItem::Type LayoutButton::type() const
{
	return BUTTON;
}

//=======

LayoutImage::LayoutImage(const QString& img, const QString& name, const QString& text, const QString& tooltip, int x_, int y_, int width_, int height_, int angle_):
LayoutButton(name, text, tooltip, x_, y_, width_, height_,  angle_),
image(img)
{
	
}

LayoutItem::Type LayoutImage::type() const
{
	return BUTTON;
}


QString LayoutImage::getImage() const
{
	return image;
}

//=======================================================================================================

LayoutContent::LayoutContent():
m_cr(CLDeviceCriteria::NONE),
mDecoration(0),
m_recorder(false),
m_editable(false)
{
	
}

LayoutContent::~LayoutContent()
{

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

void LayoutContent::addDevice(const QString& uniqueId, int x, int y, int width, int height, int angle = 0)
{
	m_devices.push_back(new LayoutDevice(uniqueId, x, y, width, height, angle));
}

void LayoutContent::addLayout(LayoutContent* l)
{
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

QList<LayoutContent*>& LayoutContent::childrenList() 
{
	return m_childlist;
}

//=========================================================================

LayoutContent* LayoutContent::coppyLayout(LayoutContent* l)
{
	LayoutContent* result = new LayoutContent();
	*result = *l; // this copy everything including lists of pointers; we do not need that;

	

}