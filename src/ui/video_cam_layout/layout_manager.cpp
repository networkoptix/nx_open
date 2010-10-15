#include "layout_manager.h"
#include "start_screen_content.h"
#include "layout_content.h"



CLSceneLayoutManager::CLSceneLayoutManager()
{
	mRootContent = new LayoutContent();
	mRootContent->addDecorationFlag(LayoutContent::HomeButton | LayoutContent::BackGroundLogo );
	load();
}

CLSceneLayoutManager::~CLSceneLayoutManager()
{
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
	LayoutContent* cont = new LayoutContent();
	cont->addDecorationFlag(LayoutContent::HomeButton | LayoutContent::BackGroundLogo | LayoutContent::LevelUp);

	CLDeviceCriteria cr(CLDeviceCriteria::ALL);
	cr.setRecorderId(id);
	cont->setDeviceCriteria(cr);

	cont->setName(id);
	cont->setRecorder();

	mRecordersContents[id] = cont;

	mRootContent->addLayout(cont);

}

LayoutContent* CLSceneLayoutManager::getRecorderLyoutContent( QString id )
{
	if (!mRecordersContents.count(id))
		return 0;

	return mRecordersContents[id];
}

LayoutContent* CLSceneLayoutManager::getAllLayoutsContent()
{
	return mRootContent;
}