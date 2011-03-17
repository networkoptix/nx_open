#ifndef layout_items_h_2210
#define layout_items_h_2210

#include "device\device_managmen\device_manager.h"

class CLCustomBtnItem;
class CLStaticImageItem;

class LayoutItem
{
public:
	enum Type {DEVICE, BUTTON, LAYOUT, IMAGE, BACKGROUND};
	LayoutItem();
	LayoutItem(int x, int y, int width, int height, int angle = 0);

	static QString Type2String(Type t);
	static Type String2Type(const QString& str);

	virtual void toXml(QDomDocument& doc, QDomElement& parent)  = 0;

	virtual ~LayoutItem();
	virtual Type type() const = 0;

	int getX() const;
	int getY() const;
	QPointF pos() const;

	int width() const;
	int height() const;
	QSize size() const;

	int angle() const;

	void setName(const QString& name);
	QString getName() const;

protected:
	int x, y;
	int w, h;
	int angl;
	QString name;
};
//=======

class LayoutDevice : public LayoutItem
{
public:
	LayoutDevice();
	LayoutDevice(const QString& uniqueId, int x, int y, int width, int height, int angle = 0);
	virtual Type type() const;
	QString getId() const;
	virtual void toXml(QDomDocument& doc, QDomElement& parent) ;
protected:
	QString id;
};

//=======
class LayoutButton: public LayoutItem
{
public:
	LayoutButton();
	LayoutButton(const QString& name, const QString& text, const QString& tooltip, int x, int y, int width, int height, int angle = 0);
	virtual Type type() const;
	virtual void toXml(QDomDocument& doc, QDomElement& parent) ;
protected:
	QString m_text;
	QString m_tooltip;
};
//=======

class LayoutImage: public LayoutButton
{
public:
	LayoutImage();
	LayoutImage(const QString& img, const QString& name, const QString& text, const QString& tooltip, int x, int y, int width, int height, int angle = 0);

	virtual Type type() const ;
	QString getImage() const;
	virtual void toXml(QDomDocument& doc, QDomElement& parent) ;
protected:
	QString image;

};

//=======================================================================================================
#endif //layout_items_h_2210
