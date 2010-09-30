#include "layout_content.h"


LayoutContentItem::LayoutContentItem(int x_, int y_, int slotx, int sloty, int width_, int height_, int angle_):
x(x_),
y(y_),
w(width_),
h(height_),
angl(angle_),
slotX(slotx),
slotY(sloty)
{

}


LayoutContentItem::~LayoutContentItem()
{

}

int LayoutContentItem::getSlotX() const
{
	return slotX;
}

int LayoutContentItem::getSlotY() const
{
	return slotY;
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

LayoutDevice::LayoutDevice(const QString& uniqueId, int x_, int y_, int slotx, int sloty, int width_, int height_, int angle_):
LayoutContentItem(x_, y_, slotx, sloty, width_, height_,  angle_),
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
LayoutButton::LayoutButton(const QString& name_, int x_, int y_, int slotx, int sloty, int width_, int height_, int angle_):
LayoutContentItem(x_, y_, slotx, sloty, width_, height_,  angle_),
name(name_)
{

}

LayoutContentItem::Type LayoutButton::type() const
{
	return BUTTON;
}


QString LayoutButton::getName() const
{
	return name;
}
//=======

LayoutImage::LayoutImage(const QString& img1, const QString& img2, int x_, int y_, int slotx, int sloty, int width_, int height_, int angle_):
LayoutContentItem(x_, y_, slotx, sloty, width_, height_,  angle_),
image1(img1),
image2(img2)
{

}

LayoutContentItem::Type LayoutImage::type() const
{
	return BUTTON;
}


QString LayoutImage::getImage1() const
{
	return image1;
}

QString LayoutImage::getImage2() const
{
	return image2;
}

//=======================================================================================================

LayoutContent::LayoutContent()
{
	m_cr.mCriteria = CLDeviceCriteria::NONE;
}

LayoutContent::~LayoutContent()
{

}

void LayoutContent::addButton(const QString& name, int x, int y, int slotx, int sloty, int width, int height, int angle)
{
	m_btns.push_back(LayoutButton(name, x, y, slotx, sloty, width, height, angle) );
}

void LayoutContent::addButton(const CLCustomBtnItem* item)
{

}

void LayoutContent::addImage(const QString& img1, const QString& img2, int slotx, int sloty, int x, int y, int width, int height, int angle)
{
	m_imgs.push_back(LayoutImage(img1, img2, x, y, slotx, sloty, width, height, angle));
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
