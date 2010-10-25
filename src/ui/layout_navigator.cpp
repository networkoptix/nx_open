#include "layout_navigator.h"
#include "video_cam_layout\start_screen_content.h"
#include "video_cam_layout\layout_manager.h"
#include "camera\gl_renderer.h"

extern QString start_screen;

QString video_layout = "video layout";

QString button_layout = "layout";
QString button_logo = "logo";
QString button_home = "home";
QString button_system = "system";
QString button_level_up = "level_up";


CLLayoutNavigator::CLLayoutNavigator(MainWnd* mainWnd, LayoutContent* content):
m_videoView(mainWnd),
mCurrentContent(content)
{
	// layout stoped go to the new one
	connect(&m_videoView.getCamLayOut(), SIGNAL(stoped(LayoutContent*)), this, SLOT(onLayOutStoped(LayoutContent*))); 


	// something like logo pressed
	connect(&m_videoView.getCamLayOut(), SIGNAL(onItemPressed(LayoutContent*, QString)), this, SLOT(onButtonItemPressed(LayoutContent*, QString))); 
	

	// home, levelup button or so
	connect(&m_videoView, SIGNAL(onDecorationPressed(LayoutContent*, QString)), this, SLOT(onDecorationPressed(LayoutContent*, QString)));

	
	// some layout ref pressed
	connect(&m_videoView.getCamLayOut(), SIGNAL(onNewLayoutSelected(LayoutContent*, LayoutContent*)), this, SLOT(onNewLayoutSelected(LayoutContent*, LayoutContent*)));

	// for layout editor	
	connect(&m_videoView.getCamLayOut(), SIGNAL(onNewLayoutItemSelected(LayoutContent*)), this, SLOT(onNewLayoutItemSelected(LayoutContent*)));



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
	CLGLRenderer::clearGarbage();
}

void CLLayoutNavigator::setMode(ViewMode mode)
{
	m_mode = mode;
}

ViewMode CLLayoutNavigator::getMode() const 
{
	return m_mode;
}

GraphicsView& CLLayoutNavigator::getView()
{
	return m_videoView;
}

void CLLayoutNavigator::goToNewLayoutContent(LayoutContent* newl)
{
	mNewContent = newl;
	goToNewLayoutContent();
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
		if (m_mode==NORMAL_ViewMode)
			goToNewLayoutContent();
		else
			emit onItemPressed(button_home);
	}
	else if (itemname==button_level_up)
	{
		if (layout->getParent()!=0)
		{
			mNewContent = layout->getParent();
			goToNewLayoutContent();
		}
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

		if (itemname==button_system)
		{
			mNewContent = CLSceneLayoutManager::instance().getAllRecordersContent();
			goToNewLayoutContent();
		}
		

		if (itemname==button_layout)
		{
			emit onItemPressed(button_layout);
		}
	}
}

void CLLayoutNavigator::onNewLayoutSelected(LayoutContent* oldl, LayoutContent* newl)
{
	if (newl != mNewContent)
	{
		mNewContent = newl;
		goToNewLayoutContent();
	}

}

void CLLayoutNavigator::onLayOutStoped(LayoutContent* l)
{
	if (m_mode==NORMAL_ViewMode)
		CLGLRenderer::clearGarbage();

	m_videoView.getCamLayOut().setContent(mNewContent);
	m_videoView.getCamLayOut().start();

}

void CLLayoutNavigator::onNewLayoutItemSelected(LayoutContent* newl)
{
	if (m_mode==LAYOUTEDITOR_ViewMode)
		emit onNewLayoutItemSelected(this, newl);
}
