#ifndef layout_content_h_2210
#define layout_content_h_2210

#include <QPointF>
#include <QSize>
#include <QString>
#include <QList>
#include "device\device_managmen\device_manager.h"

class CLCustomBtnItem;
class CLStaticImageItem;

class LayoutContentItem
{
public:
	enum Type {DEVICE, BUTTON, SHORTCUT, IMAGE, BACKGROUND};
	LayoutContentItem(int x, int y, int width, int height, int angle = 0);
	virtual ~LayoutContentItem();
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

class LayoutDevice : public LayoutContentItem
{
public:
	LayoutDevice(const QString& uniqueId, int x, int y, int width, int height, int angle = 0);

	virtual Type type() const;
	
	QString getId() const;
protected:
	QString id;
};

//=======
class LayoutButton: public LayoutContentItem
{
public:
	LayoutButton(const QString& name, int x, int y, int width, int height, int angle = 0);
	virtual Type type() const;

};
//=======

class LayoutImage: public LayoutContentItem
{
public:
	LayoutImage(const QString& img, const QString& name, int x, int y, int width, int height, int angle = 0);

	virtual Type type() const ;
	QString getImage() const;
protected:
	QString image;

};

//=======================================================================================================


class LayoutContent
{
public:
	LayoutContent();
	virtual ~LayoutContent();

	void addButton(const QString& name, int x, int y, int width, int height, int angle = 0);
	void addButton(const CLCustomBtnItem* item);

	void addImage(const QString& img, const QString& name, int x, int y, int width, int height, int angle = 0);
	void addImage(const CLStaticImageItem* item);

	void setDeviceCriteria(const CLDeviceCriteria& cr);
	CLDeviceCriteria getDeviceCriteria() const;

	QList<LayoutImage> getImages() const;
	QList<LayoutButton> getButtons() const;
protected:
	QList<LayoutImage> m_imgs;
	QList<LayoutButton> m_btns;
	CLDeviceCriteria m_cr;

};
#endif //layout_content_h_2210
