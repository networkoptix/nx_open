#include "layout_items.h"


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
