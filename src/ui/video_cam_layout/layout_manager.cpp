#include "layout_manager.h"
#include "start_screen_content.h"
#include "layout_content.h"



CLSceneLayoutManager::CLSceneLayoutManager()
{
	mRootContent = new LayoutContent();
	mRootContent->addDecorationFlag(LayoutContent::HomeButton | LayoutContent::BackGroundLogo );
	load();

	mAllRecorders = new LayoutContent();
	mAllRecorders->addDecorationFlag(LayoutContent::HomeButton | LayoutContent::BackGroundLogo );

	mEmptyLayout = getNewEmptyLayoutContent();

}

CLSceneLayoutManager::~CLSceneLayoutManager()
{
	foreach(LayoutContent* cont, mCustomContents)
	{
		delete cont;
	}
	

	delete mRootContent;
	delete mAllRecorders;
	delete mEmptyLayout;


	save();
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

}

LayoutContent* CLSceneLayoutManager::startScreenLayoutContent()
{
	return &(startscreen_content());
}

bool CLSceneLayoutManager::hasCustomLayoutsContent() const
{
	return (mCustomContents.count()>0);
}

LayoutContent* CLSceneLayoutManager::getDefaultLayoutContent()
{
	return 0;
}

void CLSceneLayoutManager::addRecorderLayoutContent( QString id )
{
	LayoutContent* cont1 = createRecorderContent_helper(id);
	mRootContent->addLayout(cont1, false);

	LayoutContent* cont2 = createRecorderContent_helper(id);
	mAllRecorders->addLayout(cont2, false);

}

LayoutContent* CLSceneLayoutManager::getAllRecordersContent()
{
	return mAllRecorders;
}

LayoutContent* CLSceneLayoutManager::getAllLayoutsContent()
{
	return mRootContent;
}

LayoutContent* CLSceneLayoutManager::getNewEmptyLayoutContent()
{
	LayoutContent* cont = new LayoutContent();
	cont->addDecorationFlag(LayoutContent::HomeButton | LayoutContent::BackGroundLogo);

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
LayoutContent* CLSceneLayoutManager::createRecorderContent_helper(QString id)
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