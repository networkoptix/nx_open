#include "layout_content.h"

CLUserGridSettings::CLUserGridSettings():
max_rows(5),
item_distance(0.36*scale_factor),
optimal_ratio(17.0/9*scale_factor)
{

}

//============================================

LayoutContent::LayoutContent():
m_cr(CLDeviceCriteria::STATIC),
mDecoration(0),
m_recorder(false),
m_editable(false),
m_parent(0),
m_interaction_flags(0xffffffff)
{
	setName("Layout:unnamed");
}

LayoutContent::~LayoutContent()
{
    destroy(false);
}


void LayoutContent::destroy(bool skipSubL)
{
    foreach (LayoutImage* item, m_imgs)
    {
        delete item;
    }
    m_imgs.clear();

    foreach (LayoutButton* item, m_btns)
    {
        delete item;
    }
    m_btns.clear();

    foreach (LayoutDevice* item, m_devices)
    {
        delete item;
    }
    m_devices.clear();

    if (!skipSubL)
    {
        foreach (LayoutContent* cont, m_childlist)
        {
            delete cont;
        }
        m_childlist.clear();
    }
    

}

LayoutItem::Type LayoutContent::type() const
{
	return LAYOUT;
}

bool LayoutContent::isRecorder() const
{
	return m_recorder;
}

void LayoutContent::setRecorder()
{
	m_recorder = true;
}

bool LayoutContent::isEditable() const
{
	return m_editable;
}

void LayoutContent::setEditable(bool editable) 
{
	m_editable = editable;
}

CLUserGridSettings& LayoutContent::getGridSettings()
{
    return mGridSettings;
}

bool LayoutContent::hasSuchSublayoutName(const QString& name) const
{
	foreach (LayoutContent* cont, m_childlist)
	{
		if (cont->getName()==name)
			return true;
	}

	return false;
}

LayoutItem* LayoutContent::getItemByname(const QString& name) const
{
    foreach (LayoutDevice* cont, m_devices)
    {
        if (cont->getName()==name)
            return cont;
    }



    foreach (LayoutButton* cont, m_btns)
    {
        if (cont->getName()==name)
            return cont;
    }



    foreach (LayoutImage* cont, m_imgs)
    {
        if (cont->getName()==name)
            return cont;
    }



    foreach (LayoutContent* cont, m_childlist)
    {
        if (cont->getName()==name)
            return cont;
    }

    return 0;

}


bool LayoutContent::checkDecorationFlag(unsigned int flag) const
{
	return mDecoration & flag;
}

void LayoutContent::addDecorationFlag(unsigned int flag)
{
	mDecoration |= flag;
}

void LayoutContent::removeDecorationFlag(unsigned int flag)
{
	mDecoration &= ~flag;
}

void LayoutContent::setParent(LayoutContent* parent)
{
	m_parent = parent;
}

LayoutContent* LayoutContent::getParent() const
{
	return m_parent;
}

LayoutButton* LayoutContent::addButton(const QString& text, const QString& tooltip, const CLBasicLayoutItemSettings& setting)
{
    LayoutButton* result = new LayoutButton(text, tooltip, setting);
	m_btns.push_back(result);
    return result;
}

LayoutImage* LayoutContent::addImage(const QString& img, const QString& text, const QString& tooltip, const CLBasicLayoutItemSettings& setting)
{
    LayoutImage* result = new LayoutImage(img, text, tooltip, setting);
	m_imgs.push_back(result);
    return result;
}

LayoutDevice* LayoutContent::addDevice(const QString& uniqueId, const CLBasicLayoutItemSettings& setting )
{
    LayoutDevice* result = dynamic_cast<LayoutDevice*>(getItemByname(uniqueId));
    if (result)
        return result;

    result = new LayoutDevice(uniqueId, setting);
	m_devices.push_back(result);
    return result;
}

void LayoutContent::removeDevice(const QString& uniqueId)
{
	foreach(LayoutDevice* device, m_devices)
	{
		if (device->getId()==uniqueId)
		{
			m_devices.removeOne(device);
			return;
		}

	}
}

LayoutContent* LayoutContent::addLayout(LayoutContent* l, bool copy)
{
	if (copy)
		m_childlist.push_back(coppyLayoutContent(l));
	else
		m_childlist.push_back(l);

	m_childlist.back()->setParent(this);

	return m_childlist.back();

}

void LayoutContent::removeLayout(LayoutContent* l, bool del)
{
	m_childlist.removeOne(l);
	if (del) 
		delete l;
}

void LayoutContent::setDeviceCriteria(const CLDeviceCriteria& cr)
{
	m_cr = cr;
}

CLDeviceCriteria LayoutContent::getDeviceCriteria() const
{
	return m_cr;
}

QList<LayoutImage*>& LayoutContent::getImages() 
{
	return m_imgs;
}

QList<LayoutButton*>& LayoutContent::getButtons() 
{
	return m_btns;
}

QList<LayoutDevice*>& LayoutContent::getDevices()
{
	return m_devices;
}

QList<LayoutContent*>& LayoutContent::childrenList() 
{
	return m_childlist;
}

void LayoutContent::toXml(QDomDocument& doc, QDomElement& parent) 
{
	QDomElement element = doc.createElement("Layout");
	element.setAttribute("type", Type2String(type()));
	element.setAttribute("name", getName());
	element.setAttribute("recorder", (int)isRecorder());

    element.setAttribute("max_rows", mGridSettings.max_rows);
    element.setAttribute("item_distance", mGridSettings.item_distance);
    element.setAttribute("optimal_ratio", mGridSettings.optimal_ratio);

	parent.appendChild(element);

	QList<LayoutDevice*>& dev_lst = getDevices();
	foreach(LayoutDevice* dev, dev_lst)
	{
		dev->toXml(doc, element);
	}

	QList<LayoutContent*>& l_lst = childrenList();
	foreach(LayoutContent* l, l_lst)
	{
		l->toXml(doc, element);
	}
}

unsigned int LayoutContent::numberOfPages(unsigned int maxItemsPerPage)
{
    unsigned int size = getDevices().size();
    unsigned int result = ((size % maxItemsPerPage) == 0) ? size / maxItemsPerPage : size / maxItemsPerPage + 1;
    return qMax(result , (unsigned int)1);
}

QStringList LayoutContent::getDevicesOnPage(unsigned int page, unsigned int maxItemsPerPage)
{
    QStringList result;
    if (page > numberOfPages(maxItemsPerPage) )
        return result;

    QList<LayoutDevice*>& devlst = getDevices();

    unsigned int start = (page - 1) * maxItemsPerPage;
    unsigned int end = qMin((page)*maxItemsPerPage, (unsigned int)devlst.size());

    

    for (int i = start; i < end; ++i)
        result.push_back( devlst.at(i)->getId() );

    return result;

}


CLRectAdjustment LayoutContent::getRectAdjustment() const
{
	return m_adjustment;
}

void LayoutContent::setRectAdjustment(const CLRectAdjustment& adjust)
{
	m_adjustment = adjust;
}

void LayoutContent::addIntereactionFlag(unsigned long flag)
{
	m_interaction_flags |= flag;
}

void LayoutContent::removeIntereactionFlag(unsigned long flag)
{
	m_interaction_flags &= (~flag);
}

bool LayoutContent::checkIntereactionFlag(unsigned long flag)
{
	return m_interaction_flags & flag;
}

//=========================================================================

LayoutContent* LayoutContent::coppyLayoutContent(LayoutContent* l, bool skipSubL)
{
	LayoutContent* result = new LayoutContent();
    coppyLayoutContent(result, l, skipSubL);

	return result;
}

void LayoutContent::coppyLayoutContent(LayoutContent* to, LayoutContent* from, bool skipSubL)
{
    to->destroy(skipSubL);


    QList<LayoutContent*> lst_tmp = to->m_childlist;

    *to = *from; // this copy everything including lists of pointers; we do not need that;
    
    to->m_imgs.clear();
    to->m_btns.clear();
    to->m_devices.clear();

    if (!skipSubL)
        to->m_childlist.clear();
    else
        to->m_childlist = lst_tmp;

    /**/

    foreach (LayoutImage* item, from->m_imgs)
    {
        LayoutImage* iteml = new LayoutImage();
        *iteml = *item;
        to->m_imgs.push_back(iteml);
    }

    foreach (LayoutButton* item, from->m_btns)
    {
        LayoutButton* iteml = new LayoutButton();
        *iteml = *item;
        to->m_btns.push_back(iteml);
    }

    foreach (LayoutDevice* item, from->m_devices)
    {
        LayoutDevice* iteml = new LayoutDevice();
        *iteml = *item;
        to->m_devices.push_back(iteml);
    }

    if (!skipSubL)
    {
        foreach (LayoutContent* cont, from->m_childlist)
        {
            LayoutContent* contl = coppyLayoutContent(cont);
            contl->setParent(to);
            to->m_childlist.push_back(contl);
        }
    }

}