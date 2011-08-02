#include "layout_items.h"

CLBasicLayoutItemSettings::CLBasicLayoutItemSettings()
    : coordType(Undefined),
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
        return QLatin1String("Pixels");

    case Slots:
        return QLatin1String("Slots");

    default:
        return QLatin1String("Undefined");

    }
}

void CLBasicLayoutItemSettings::setCorrdType(const QString& type)
{
    if (type == QLatin1String("Pixels"))
        coordType = Pixels;
    else if (type == QLatin1String("Slots"))
        coordType = Slots;
    else
        coordType = Undefined;
}


//==================================================
LayoutItem::LayoutItem()
{
}

LayoutItem::LayoutItem(const CLBasicLayoutItemSettings& setting)
    : mSettings(setting)
{
}

LayoutItem::~LayoutItem()
{
}

QString LayoutItem::Type2String(Type t)
{
	enum Type { DEVICE, BUTTON, LAYOUT, IMAGE, BACKGROUND };

	switch(t)
	{
	case DEVICE:
		return QLatin1String("DEVICE");

	case BUTTON:
		return QLatin1String("BUTTON");

	case LAYOUT:
		return QLatin1String("LAYOUT");

	case IMAGE:
		return QLatin1String("IMAGE");

	case BACKGROUND:
		return QLatin1String("BACKGROUND");

	default:
		break;

	}
	return QLatin1String("NONE");
}

LayoutItem::Type LayoutItem::String2Type(const QString& str)
{
	if (str == QLatin1String("DEVICE"))
		return DEVICE;

	if (str == QLatin1String("BUTTON"))
		return BUTTON;

	if (str == QLatin1String("LAYOUT"))
		return LAYOUT;

	if (str == QLatin1String("IMAGE"))
		return IMAGE;

	if (str == QLatin1String("BACKGROUND"))
		return BACKGROUND;

	return IMAGE;

}

void LayoutItem::toXml(QDomDocument& doc, QDomElement& parent)
{
	QDomElement element = doc.createElement(QLatin1String("item"));
	element.setAttribute(QLatin1String("type"), Type2String(type()));
	element.setAttribute(QLatin1String("name"), getName());
	element.setAttribute(QLatin1String("coordType"), mSettings.coordTypeS());
	element.setAttribute(QLatin1String("pos_x"), mSettings.pos_x);
	element.setAttribute(QLatin1String("pos_y"), mSettings.pos_y);
	element.setAttribute(QLatin1String("angle"), mSettings.angle);
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

LayoutDevice::~LayoutDevice()
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
