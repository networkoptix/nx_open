#ifndef layout_content_h_2210
#define layout_content_h_2210

#include <QPointF>
#include <QSize>
#include <QString>
#include <QList>
#include "device\device_managmen\device_manager.h"

class CLCustomBtnItem;
class CLStaticImageItem;



class LayoutItem
{
public:
	enum Type {DEVICE, BUTTON, LAYOUT, IMAGE, BACKGROUND};
	LayoutItem();
	LayoutItem(int x, int y, int width, int height, int angle = 0);
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
	LayoutDevice(const QString& uniqueId, int x, int y, int width, int height, int angle = 0);
	virtual Type type() const;
	QString getId() const;
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
protected:
	QString m_text;
	QString m_tooltip;
};
//=======

class LayoutImage: public LayoutButton
{
public:
	LayoutImage(const QString& img, const QString& name, const QString& text, const QString& tooltip, int x, int y, int width, int height, int angle = 0);

	virtual Type type() const ;
	QString getImage() const;
protected:
	QString image;

};

//=======================================================================================================


class LayoutContent : public LayoutButton
{
public:

	enum {HomeButton = 0x01, LevelUp = 0x02, BackGroundLogo = 0x04};

	LayoutContent();
	virtual ~LayoutContent();

	virtual Type type() const;

	bool isEditable() const;
	void setEditable(bool editable) ;

	bool isRecorder() const;
	void setRecorder();

	bool checkDecorationFlag(unsigned int flag) const;
	void addDecorationFlag(unsigned int flag);

	void setParent(LayoutContent* parent);
	LayoutContent* getParent() const;

	void addButton(const QString& name, const QString& text, const QString& tooltip, int x, int y, int width, int height, int angle = 0);
	void addImage(const QString& img, const QString& name, const QString& text, const QString& tooltip, int x, int y, int width, int height, int angle = 0);
	void addDevice(const QString& uniqueId, int x, int y, int width, int height, int angle = 0);
	void addLayout(LayoutContent* l);


	void setDeviceCriteria(const CLDeviceCriteria& cr);
	CLDeviceCriteria getDeviceCriteria() const;

	QList<LayoutImage*>& getImages();
	QList<LayoutButton*>& getButtons();
	QList<LayoutContent*>& childrenList();
	
protected:
	LayoutContent* coppyLayout(LayoutContent* l);
protected:
	QList<LayoutImage*> m_imgs;
	QList<LayoutButton*> m_btns;
	QList<LayoutContent*> m_childlist;
	QList<LayoutDevice*> m_devices;


	CLDeviceCriteria m_cr;

	LayoutContent* m_parent;

	unsigned int mDecoration;

	bool m_recorder;
	bool m_editable;

};
#endif //layout_content_h_2210
