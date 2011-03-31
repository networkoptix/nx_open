#include "layout_items.h"

CLBasicLayoutItemSettings::CLBasicLayoutItemSettings():
coordType(Undefined),
pos_x(0),
pos_y(0),
width(0), 
height(0),
angle(0)
{
}

QString CLBasicLayoutItemSettings::coordTypeS() const
{
    switch (coordType)
    {
    case Pixels:
        return "Pixels";

    case Slots:
        return "Slots";

    default:
        return "Undefined";

    }
}

void CLBasicLayoutItemSettings::setCorrdType(const QString& type)
{
    if (type == "Pixels")
        coordType = Pixels;
    else if (type == "Slots")
            coordType = Slots;
    else 
        coordType = Undefined;
}


//==================================================
LayoutItem::LayoutItem()
{

}

LayoutItem::LayoutItem(const CLBasicLayoutItemSettings& setting):
mSettings(setting)
{

}

LayoutItem::~LayoutItem()
{

}

QString LayoutItem::Type2String(Type t)
{
	enum Type {DEVICE, BUTTON, LAYOUT, IMAGE, BACKGROUND};

	switch(t)
	{
	case DEVICE:
		return "DEVICE";

	case BUTTON:
		return "BUTTON";

	case LAYOUT:
	    return "LAYOUT";

	case IMAGE:
	    return "IMAGE";

	case BACKGROUND:
		return "BACKGROUND";

	default:
		return "NONE";

	}
	return "NONE";
}

LayoutItem::Type LayoutItem::String2Type(const QString& str)
{
	if (str=="DEVICE")
		return DEVICE;

	if (str=="BUTTON")
		return BUTTON;

	if (str=="LAYOUT")
		return LAYOUT;

	if (str=="IMAGE")
		return IMAGE;

	if (str=="BACKGROUND")
		return BACKGROUND;

	return IMAGE;

}

void LayoutItem::toXml(QDomDocument& doc, QDomElement& parent) 
{
	QDomElement element = doc.createElement("item");
	element.setAttribute("type", Type2String(type()));
	element.setAttribute("name", getName());
    element.setAttribute("coordType", mSettings.coordTypeS());
    element.setAttribute("pos_x", mSettings.pos_x);
    element.setAttribute("pos_y", mSettings.pos_y);
    element.setAttribute("angle", mSettings.angle);
	parent.appendChild(element);

}

QString LayoutItem::getName() const
{
    return mSettings.name;
}

void LayoutItem::setName(const QString& name )
{
    mSettings.name = name;
}


CLBasicLayoutItemSettings& LayoutItem::getBasicSettings()
{
    return mSettings;
}

//=======
LayoutDevice::LayoutDevice()
{

}

LayoutDevice::LayoutDevice(const QString& uniqueId, const CLBasicLayoutItemSettings& setting):
LayoutItem(setting),
id(uniqueId)
{
	setName(uniqueId);
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

LayoutButton::LayoutButton(const QString& text, const QString& tooltip, const CLBasicLayoutItemSettings& setting):
LayoutItem(setting),
m_text(text),
m_tooltip(tooltip)
{
}

LayoutItem::Type LayoutButton::type() const
{
	return BUTTON;
}


//=======
LayoutImage::LayoutImage()
{

}

LayoutImage::LayoutImage(const QString& img, const QString& text, const QString& tooltip, const CLBasicLayoutItemSettings& setting):
LayoutButton(text, tooltip, setting),
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
