#ifndef layout_items_h_2210
#define layout_items_h_2210


class CLCustomBtnItem;
class CLStaticImageItem;

struct CLBasicLayoutItemSettings
{
    CLBasicLayoutItemSettings();
    enum Coordtype{Undefined, Pixels, Slots}; 
    Coordtype coordType;
    int pos_x, pos_y;
    int width, height;
    int angle;
    QString name;

    QString coordTypeS() const;
    void setCorrdType(const QString& type);

};

class LayoutItem
{
public:
	enum Type {DEVICE, BUTTON, LAYOUT, IMAGE, BACKGROUND};
	LayoutItem();
	explicit LayoutItem(const CLBasicLayoutItemSettings& setting);

	static QString Type2String(Type t);
	static Type String2Type(const QString& str);

	virtual void toXml(QDomDocument& doc, QDomElement& parent);

	virtual ~LayoutItem();
	virtual Type type() const = 0;

    CLBasicLayoutItemSettings& getBasicSettings();

    QString getName() const;
    void  setName(const QString& name );

protected:
    CLBasicLayoutItemSettings mSettings;
};
//=======

class LayoutDevice : public LayoutItem
{
public:
	LayoutDevice();
    ~LayoutDevice();
	LayoutDevice(const QString& uniqueId, const CLBasicLayoutItemSettings& setting);
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
	LayoutButton(const QString& text, const QString& tooltip, const CLBasicLayoutItemSettings& setting);
	virtual Type type() const;
protected:
	QString m_text;
	QString m_tooltip;
};
//=======

class LayoutImage: public LayoutButton
{
public:
	LayoutImage();
	LayoutImage(const QString& img, const QString& text, const QString& tooltip, const CLBasicLayoutItemSettings& setting);

	virtual Type type() const ;
	QString getImage() const;
protected:
	QString image;

};

//=======================================================================================================
#endif //layout_items_h_2210
