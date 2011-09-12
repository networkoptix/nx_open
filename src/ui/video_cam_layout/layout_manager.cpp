#include "util.h"
#include "layout_manager.h"
#include "start_screen_content.h"
#include "layout_content.h"

#include "base/log.h"
#include "intro_screen_content.h"

CLSceneLayoutManager::CLSceneLayoutManager():
mRecordersAndLayouts(0)
{
	mAllCustomLayouts = new LayoutContent();
	mAllCustomLayouts->addDecorationFlag(
		LayoutContent::SettingButton |
		LayoutContent::BackGroundLogo |
		LayoutContent::MagnifyingGlass |
		LayoutContent::SquareLayout |
		LayoutContent::LongLayout |
		LayoutContent::SingleLineLayout |
        LayoutContent::ToggleFullScreenButton |
        LayoutContent::ExitButton |
		LayoutContent::MultiPageSelection );

    mAllCustomLayouts->removeIntereactionFlag(
        LayoutContent::Zoomable |
        LayoutContent::SceneMovable |
        LayoutContent::ShowAvalable |
        LayoutContent::ItemMovable |
        LayoutContent::GridEnable |
        LayoutContent::ItemRotatable);


	mAllCustomLayouts->setEditable(true);
	load();

	mAllRecorders = new LayoutContent();
	mAllRecorders->addDecorationFlag(
		LayoutContent::SettingButton |
		LayoutContent::BackGroundLogo |
		LayoutContent::MagnifyingGlass |
		LayoutContent::SquareLayout |
		LayoutContent::LongLayout |
		LayoutContent::SingleLineLayout |
        LayoutContent::ToggleFullScreenButton |
        LayoutContent::ExitButton |
		LayoutContent::MultiPageSelection);

	mEmptyLayout = getNewEmptyLayoutContent();

    mSearchLayout = getNewEmptyLayoutContent(
        LayoutContent::SettingButton |
        LayoutContent::BackGroundLogo |
        LayoutContent::SquareLayout |
        LayoutContent::LongLayout |
        LayoutContent::SingleLineLayout |
        LayoutContent::ToggleFullScreenButton |
        LayoutContent::ExitButton |
        LayoutContent::MultiPageSelection);

	mSearchLayout->removeDecorationFlag(LayoutContent::MagnifyingGlass);
	mSearchLayout->addDecorationFlag(LayoutContent::SearchEdit);

}

CLSceneLayoutManager::~CLSceneLayoutManager()
{
	save();

	delete mAllCustomLayouts;
	delete mAllRecorders;
	delete mEmptyLayout;
	delete mRecordersAndLayouts;
}

CLSceneLayoutManager& CLSceneLayoutManager::instance()
{
	static CLSceneLayoutManager inst;
	return inst;
}

bool CLSceneLayoutManager::load()
{
	QString dataLocation = getDataDirectory();

	QFile file(dataLocation + QLatin1String("/custom_layouts.xml"));

	if (!file.exists())
	{
		return false;
	}

	QString errorStr;
	int errorLine;
	int errorColumn;
	QString error;
	QDomDocument doc;

	if (!doc.setContent(&file, &errorStr, &errorLine,&errorColumn))
	{
		QTextStream str(&error);
		str << "File custom_layouts.xml" << "; Parse error at line " << errorLine << " column " << errorColumn << " : " << errorStr;
		cl_log.log(error, cl_logERROR);
		return false;
	}

	QDomElement root = doc.documentElement();
	if (root.tagName() != QLatin1String("CustomLayouts"))
		return false;

	QDomNode node = root.firstChild();

	while (!node.isNull())
	{
		if (node.toElement().tagName() == QLatin1String("Layout"))
		{
			if (!load_parseLyout(node.toElement(), mAllCustomLayouts))
				return false;
		}

		node = node.nextSibling();
	}

	return true;
}

bool CLSceneLayoutManager::load_parseLyout(const QDomElement& layout, LayoutContent* parent)
{
	LayoutContent* lc = 0;
	QString name = layout.attribute(QLatin1String("name"));

	QString recorder = layout.attribute(QLatin1String("recorder"));
	if (recorder == QLatin1String("1"))
	{
		lc = createRecorderContent(name);
	}
	else
	{
		lc = getNewEmptyLayoutContent();
		lc->setName(name);
	}

    CLUserGridSettings& gs = lc->getGridSettings();
    gs.max_rows = layout.attribute(QLatin1String("max_rows")).toInt();
    gs.item_distance = layout.attribute(QLatin1String("item_distance")).toInt();
    gs.optimal_ratio = layout.attribute(QLatin1String("optimal_ratio")).toInt();


	parent->addLayout(lc, false);

	//=========
	QDomNode node = layout.firstChild();

	while (!node.isNull())
	{
		if (node.toElement().tagName() == QLatin1String("Layout"))
		{
			if (!load_parseLyout(node.toElement(), lc))
				return false;
		}

		if (node.toElement().tagName() == QLatin1String("item"))
		{
			QString type = node.toElement().attribute(QLatin1String("type"));
			QString name = node.toElement().attribute(QLatin1String("name"));



			if (type == QLatin1String("DEVICE"))
			{
				LayoutDevice* dev = lc->addDevice(name);

                CLBasicLayoutItemSettings& bs = dev->getBasicSettings();
                bs.coordType = CLBasicLayoutItemSettings::Slots;
                bs.pos_x = node.toElement().attribute(QLatin1String("pos_x")).toInt();
                bs.pos_y = node.toElement().attribute(QLatin1String("pos_y")).toInt();
                bs.angle = node.toElement().attribute(QLatin1String("angle")).toInt();
            }
        }

		node = node.nextSibling();
	}

	return true;
}

void CLSceneLayoutManager::save()
{
	QDomDocument doc;
	QDomElement root = doc.createElement(QLatin1String("CustomLayouts"));
	doc.appendChild(root);

	QList<LayoutContent*>& custom_layots_list = mAllCustomLayouts->childrenList();
	foreach(LayoutContent* cl, custom_layots_list)
	{
		cl->toXml(doc, root);
	}

	QString xml = doc.toString();

	QString dataLocation = getDataDirectory();
	QFile file(dataLocation + QLatin1String("/custom_layouts.xml"));
	file.open(QIODevice::WriteOnly);

	QTextStream fstr(&file);
	fstr<< xml;
	fstr.flush();

}

LayoutContent* CLSceneLayoutManager::introScreenLayoutContent()
{
	return &(intro_screen_content());
}

LayoutContent* CLSceneLayoutManager::startScreenLayoutContent()
{
	return &(startscreen_content());
}

LayoutContent* CLSceneLayoutManager::getDefaultLayoutContent()
{
	return 0;
}

void CLSceneLayoutManager::addRecorderLayoutContent( QString id )
{
	LayoutContent* cont = createRecorderContent(id);
	mAllRecorders->addLayout(cont, false);
}

LayoutContent* CLSceneLayoutManager::getAllRecordersContent()
{
	return mAllRecorders;
}

LayoutContent* CLSceneLayoutManager::getAllLayoutsContent()
{
	return mAllCustomLayouts;
}

LayoutContent* CLSceneLayoutManager::generateAllRecordersAndLayouts()
{
	delete mRecordersAndLayouts;

	mRecordersAndLayouts = new LayoutContent();
	CLDeviceCriteria cr(CLDeviceCriteria::STATIC);
	mRecordersAndLayouts->setDeviceCriteria(cr);

	QList<LayoutContent*>& childrenlst1 = mAllCustomLayouts->childrenList();
	foreach(LayoutContent* cont,  childrenlst1)
	{
		mRecordersAndLayouts->addLayout(cont, true);
	}

	QList<LayoutContent*>& childrenlst2 = mAllRecorders->childrenList();
	foreach(LayoutContent* cont,  childrenlst2)
	{
		mRecordersAndLayouts->addLayout(cont, true);
	}

	return mRecordersAndLayouts;
}

LayoutContent* CLSceneLayoutManager::getNewEmptyLayoutContent(unsigned int flags )
{
	LayoutContent* cont = new LayoutContent();
	cont->addDecorationFlag(
		LayoutContent::SettingButton |
		LayoutContent::BackGroundLogo |
		LayoutContent::MagnifyingGlass |
		LayoutContent::SquareLayout |
		LayoutContent::LongLayout |
		LayoutContent::SingleLineLayout |
        LayoutContent::ToggleFullScreenButton |
        LayoutContent::ExitButton |
		LayoutContent::MultiPageSelection | flags);

	CLDeviceCriteria cr(CLDeviceCriteria::STATIC);
	cont->setDeviceCriteria(cr);

	cont->setEditable(true);

	return cont;
}

LayoutContent* CLSceneLayoutManager::getEmptyLayoutContent()
{
	return mEmptyLayout;
}

LayoutContent* CLSceneLayoutManager::getSearchLayout()
{
	return mSearchLayout;
}

//===============================================================================
LayoutContent* CLSceneLayoutManager::createRecorderContent(QString id)
{
	LayoutContent* cont = new LayoutContent();
	cont->addDecorationFlag(
		LayoutContent::SettingButton |
		LayoutContent::BackGroundLogo |
		LayoutContent::MagnifyingGlass |
		LayoutContent::SquareLayout |
		LayoutContent::LongLayout |
		LayoutContent::SingleLineLayout |
		LayoutContent::MultiPageSelection |
        LayoutContent::ToggleFullScreenButton |
        LayoutContent::ExitButton |
		LayoutContent::LevelUp);

	CLDeviceCriteria cr(CLDeviceCriteria::ALL);
	cr.setRecorderId(id);
	cont->setDeviceCriteria(cr);

	cont->setName(id);
	cont->setRecorder();

	return cont;

}
