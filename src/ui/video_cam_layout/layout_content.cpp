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
mDecoration(0)
{
	
}

LayoutContent::~LayoutContent()
{

}

LayoutItem::Type LayoutContent::type() const
{
	return LAYOUT;
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

void LayoutContent::addButton(const QString& name, const QString& text, const QString& tooltip, int x, int y, int width, int height, int angle)
{
	m_btns.push_back(LayoutButton(name, text, tooltip, x, y, width, height, angle) );
}

void LayoutContent::addImage(const QString& img, const QString& name, const QString& text, const QString& tooltip, int x, int y, int width, int height, int angle)
{
	m_imgs.push_back(LayoutImage(img, name, text, tooltip, x, y, width, height, angle));
}

void LayoutContent::addLayout(const LayoutContent& l)
{
	m_childlist.push_back(l);
	m_childlist.back().setParent(this);
}


void LayoutContent::setDeviceCriteria(const CLDeviceCriteria& cr)
{
	m_cr = cr;
}

CLDeviceCriteria LayoutContent::getDeviceCriteria() const
{
	return m_cr;
}


QList<LayoutImage>& LayoutContent::getImages() 
{
	return m_imgs;
}

QList<LayoutButton>& LayoutContent::getButtons() 
{
	return m_btns;
}

QList<LayoutContent>& LayoutContent::childrenList() 
{
	return m_childlist;
}