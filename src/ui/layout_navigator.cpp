#include "layout_navigator.h"
#include "video_cam_layout\start_screen_content.h"
#include "video_cam_layout\layout_manager.h"

extern QString start_screen;

QString video_layout = "video layout";

QString button_logo = "logo";
QString button_home = "home";


CLLayoutNavigator::CLLayoutNavigator(MainWnd* mainWnd, LayoutContent* content):
m_videoView(mainWnd),
mCurrentContent(content)
{
	connect(&m_videoView.getCamLayOut(), SIGNAL(stoped(LayoutContent*)), this, SLOT(onLayOutStoped(LayoutContent*)));
	connect(&m_videoView.getCamLayOut(), SIGNAL(onItemPressed(LayoutContent*, QString)), this, SLOT(onButtonItemPressed(LayoutContent*, QString)));
	connect(&m_videoView, SIGNAL(onDecorationPressed(LayoutContent*, QString)), this, SLOT(onDecorationPressed(LayoutContent*, QString)));

	if (mCurrentContent==0)
		mCurrentContent = CLSceneLayoutManager::instance().startScreenLayoutContent();

	m_videoView.getCamLayOut().setContent(mCurrentContent);
	m_videoView.getCamLayOut().start();
}

CLLayoutNavigator::~CLLayoutNavigator()
{
	destroy();
}

void CLLayoutNavigator::destroy()
{
	m_videoView.getCamLayOut().stop();
	m_videoView.closeAllDlg();
}

GraphicsView& CLLayoutNavigator::getView()
{
	return m_videoView;
}

void CLLayoutNavigator::goToNewLayoutContent()
{
	m_videoView.getCamLayOut().stop(true);
}

void CLLayoutNavigator::onDecorationPressed(LayoutContent* layout, QString itemname)
{
	if (itemname==button_home)
	{
		mNewContent = CLSceneLayoutManager::instance().startScreenLayoutContent();
		goToNewLayoutContent();
	}


}

void CLLayoutNavigator::onButtonItemPressed(LayoutContent* l, QString itemname )
{
	if (l->getName() == start_screen)
	{
		if (itemname==button_logo)
		{
			mNewContent = CLSceneLayoutManager::instance().getDefaultLayoutContent();
			if (!mNewContent) // no default so far
				mNewContent = CLSceneLayoutManager::instance().getAllLayoutsContent();

			goToNewLayoutContent();
			
		}
	}

}

void CLLayoutNavigator::onLayOutStoped(LayoutContent* l)
{
	m_videoView.getCamLayOut().setContent(mNewContent);
	m_videoView.getCamLayOut().start();
}
