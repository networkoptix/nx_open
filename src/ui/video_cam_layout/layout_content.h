#ifndef layout_content_h_2210
#define layout_content_h_2210

#include "layout_items.h"

class QDomElement;

struct CLRectAdjustment
{
	CLRectAdjustment():
	x1(0), y1(0), x2(0), y2(0)
	{

	}

	int x1, y1, x2, y2;
};

class LayoutContent : public LayoutButton
{
public:

	enum {HomeButton = 0x01, LevelUp = 0x02, BackGroundLogo = 0x04, MagnifyingGlass = 0x08, SearchEdit = 0x10, SquareLayout = 0x20,  LongLayout = 0x40};
	enum {Zoomable = 0x01, SceneMovable = 0x02, ShowAvalable = 0x04, ItemMovable = 0x08, GridEnable = 0x10, ItemRotatable = 0x20, ItemSelectable = 0x40};

	LayoutContent();
	virtual ~LayoutContent();

	virtual Type type() const;

	bool isEditable() const;
	void setEditable(bool editable) ;

	bool isRecorder() const;
	void setRecorder();

	bool hasSuchSublayoutName(const QString& name) const;

	bool checkDecorationFlag(unsigned int flag) const;
	void addDecorationFlag(unsigned int flag);
	void removeDecorationFlag(unsigned int flag);

	void setParent(LayoutContent* parent);
	LayoutContent* getParent() const;

	void addButton(const QString& name, const QString& text, const QString& tooltip, int x, int y, int width, int height, int angle = 0);
	void addImage(const QString& img, const QString& name, const QString& text, const QString& tooltip, int x, int y, int width, int height, int angle = 0);
	void addDevice(const QString& uniqueId, int x=0, int y=0, int width=0, int height=0, int angle = 0);
	void removeDevice(const QString& uniqueId);

	// return l if copy is false, else returns copy of l
	LayoutContent* addLayout(LayoutContent* l, bool copy);

	// removes such layout if exists. if del then delete it 
	void removeLayout(LayoutContent* l, bool del);

	void setDeviceCriteria(const CLDeviceCriteria& cr);
	CLDeviceCriteria getDeviceCriteria() const;

	QList<LayoutImage*>& getImages();
	QList<LayoutButton*>& getButtons();
	QList<LayoutDevice*>& getDevices();

	QList<LayoutContent*>& childrenList();

	static LayoutContent* coppyLayout(LayoutContent* l);

	void toXml(QDomDocument& doc, QDomElement& parent) ;

	CLRectAdjustment getRectAdjustment() const;
	void setRectAdjustment(const CLRectAdjustment& adjust);

	void addIntereactionFlag(unsigned long flag);
	void removeIntereactionFlag(unsigned long flag);
	bool checkIntereactionFlag(unsigned long flag);

protected:

	// scene rect adjustment
	CLRectAdjustment m_adjustment;

	QList<LayoutImage*> m_imgs;
	QList<LayoutButton*> m_btns;
	QList<LayoutDevice*> m_devices;

	QList<LayoutContent*> m_childlist;

	CLDeviceCriteria m_cr;

	LayoutContent* m_parent;

	unsigned int mDecoration;

	bool m_recorder;
	bool m_editable;

	unsigned long m_interaction_flags;

};
#endif //layout_content_h_2210
