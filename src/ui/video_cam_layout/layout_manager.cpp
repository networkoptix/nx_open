#include "layout_manager.h"
#include "start_screen_content.h"
#include "layout_content.h"

#include <QDomDocument>
#include <QFile>

CLSceneLayoutManager::CLSceneLayoutManager():
mRecordersAndLayouts(0)
{
	mAllCustomLayouts = new LayoutContent();
	mAllCustomLayouts->addDecorationFlag(LayoutContent::HomeButton | LayoutContent::BackGroundLogo );
	mAllCustomLayouts->setEditable(true);
	load();

	mAllRecorders = new LayoutContent();
	mAllRecorders->addDecorationFlag(LayoutContent::HomeButton | LayoutContent::BackGroundLogo );

	mEmptyLayout = getNewEmptyLayoutContent();

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
	return true;
}

void CLSceneLayoutManager::save()
{
	QDomDocument doc("");
	QDomElement root = doc.createElement("CustomLayouts");
	doc.appendChild(root);


	QList<LayoutContent*>& custom_layots_list = mAllCustomLayouts->childrenList();
	foreach(LayoutContent* cl, custom_layots_list)
	{
		cl->toXml(doc, root);
	}


	QString xml = doc.toString();



	QFile file("custom_layouts.xml");
	file.open(QIODevice::WriteOnly);

	QTextStream fstr(&file);
	fstr<< xml;
	fstr.flush();

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
	CLDeviceCriteria cr(CLDeviceCriteria::NONE);
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
	cont->addDecorationFlag(LayoutContent::HomeButton | LayoutContent::BackGroundLogo | flags);

	CLDeviceCriteria cr(CLDeviceCriteria::NONE);
	cont->setDeviceCriteria(cr);

	cont->setEditable(true);

	return cont;
}

LayoutContent* CLSceneLayoutManager::getEmptyLayoutContent()
{
	return mEmptyLayout;
}

//===============================================================================
LayoutContent* CLSceneLayoutManager::createRecorderContent(QString id)
{
	LayoutContent* cont = new LayoutContent();
	cont->addDecorationFlag(LayoutContent::HomeButton | LayoutContent::BackGroundLogo | LayoutContent::LevelUp);

	CLDeviceCriteria cr(CLDeviceCriteria::ALL);
	cr.setRecorderId(id);
	cont->setDeviceCriteria(cr);

	cont->setName(id);
	cont->setRecorder();

	return cont;

}