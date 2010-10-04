#include "layout_content.h"


LayoutContentItem::LayoutContentItem(int x_, int y_, int width_, int height_, int angle_):
x(x_),
y(y_),
w(width_),
h(height_),
angl(angle_)
{

}


LayoutContentItem::~LayoutContentItem()
{

}

void LayoutContentItem::setName(const QString& name)
{
	this->name = name;
}

QString LayoutContentItem::getName() const
{
	return name;
}



int LayoutContentItem::getX() const
{
	return x;
}


int LayoutContentItem::getY() const
{
	return y;
}

QPointF LayoutContentItem::pos() const
{
	return QPointF(x,y);
}


int LayoutContentItem::width() const
{
	return w;
}

int LayoutContentItem::height() const
{
	return h;
}

QSize LayoutContentItem::size() const
{
	return QSize(x,y);
}

int LayoutContentItem::angle() const
{
	return angl;
}

//=======

LayoutDevice::LayoutDevice(const QString& uniqueId, int x_, int y_, int width_, int height_, int angle_):
LayoutContentItem(x_, y_, width_, height_,  angle_),
id(uniqueId)
{

}

LayoutContentItem::Type LayoutDevice::type() const
{
	return DEVICE;
}

QString LayoutDevice::getId() const
{
	return id;
}

//=======
LayoutButton::LayoutButton(const QString& name_, int x_, int y_, int width_, int height_, int angle_):
LayoutContentItem(x_, y_, width_, height_,  angle_)
{
	setName(name_);
}

LayoutContentItem::Type LayoutButton::type() const
{
	return BUTTON;
}

//=======

LayoutImage::LayoutImage(const QString& img, const QString& name, int x_, int y_, int width_, int height_, int angle_):
LayoutContentItem(x_, y_, width_, height_,  angle_),
image(img)
{
	setName(name);
}

LayoutContentItem::Type LayoutImage::type() const
{
	return BUTTON;
}


QString LayoutImage::getImage() const
{
	return image;
}

//=======================================================================================================

LayoutContent::LayoutContent()
{
	m_cr.mCriteria = CLDeviceCriteria::ALL;
}

LayoutContent::~LayoutContent()
{

}

void LayoutContent::addButton(const QString& name, int x, int y, int width, int height, int angle)
{
	m_btns.push_back(LayoutButton(name, x, y, width, height, angle) );
}

void LayoutContent::addButton(const CLCustomBtnItem* item)
{

}

void LayoutContent::addImage(const QString& img, const QString& name, int x, int y, int width, int height, int angle)
{
	m_imgs.push_back(LayoutImage(img, name, x, y, width, height, angle));
}

void LayoutContent::addImage(const CLStaticImageItem* item)
{
	
}

void LayoutContent::setDeviceCriteria(const CLDeviceCriteria& cr)
{
	m_cr = cr;
}

CLDeviceCriteria LayoutContent::getDeviceCriteria() const
{
	return m_cr;
}


QList<LayoutImage> LayoutContent::getImages() const
{
	return m_imgs;
}

QList<LayoutButton> LayoutContent::getButtons() const
{
	return m_btns;
}
