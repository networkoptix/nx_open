#include "layout_editor_wnd.h"
#include "ui/video_cam_layout/start_screen_content.h"
#include "settings.h"
#include "ui/layout_navigator.h"
#include "ui/video_cam_layout/layout_manager.h"
#include "ui_common.h"

extern QString button_layout;
extern QString button_home;

CLLayoutEditorWnd::CLLayoutEditorWnd(LayoutContent* contexttoEdit):
QDialog(0, Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint),
m_topView(0),
m_bottomView(0),
m_editedView(0)
{
	//ui.setupUi(this);
	setWindowTitle(tr("Layout Editor"));

	setAutoFillBackground(false);

    QPalette pal = palette();
    pal.setColor(backgroundRole(), app_bkr_color);
    setPalette(pal);

	//setWindowOpacity(.90);

	//=======================================================

	const int min_wisth = 1000;
	setMinimumWidth(min_wisth);
	setMinimumHeight(min_wisth*3/4);

	//showFullScreen();

	//=======================================================

	//=======add====
	m_topView = new CLLayoutNavigator(this,  CLSceneLayoutManager::instance().generateAllRecordersAndLayouts());

	m_bottomView = new CLLayoutNavigator(this, CLSceneLayoutManager::instance().getEmptyLayoutContent());

	m_editedView = new CLLayoutNavigator(this, contexttoEdit);

	connect(m_topView, SIGNAL(onNewLayoutItemSelected(CLLayoutNavigator*, LayoutContent*)), this, SLOT(onNewLayoutItemSelected(CLLayoutNavigator*, LayoutContent*)));
	connect(m_bottomView, SIGNAL(onNewLayoutItemSelected(CLLayoutNavigator*, LayoutContent*)), this, SLOT(onNewLayoutItemSelected(CLLayoutNavigator*, LayoutContent*)));

	m_topView->setMode(LAYOUTEDITOR_ViewMode);
	m_topView->getView().setViewMode(GraphicsView::ItemsDonor);

	m_bottomView->setMode(LAYOUTEDITOR_ViewMode);
	m_bottomView->getView().setViewMode(GraphicsView::ItemsDonor);

	m_editedView->setMode(LAYOUTEDITOR_ViewMode);
	m_editedView->getView().setViewMode(GraphicsView::ItemsAcceptor);

	QLayout* ml = new QHBoxLayout();

	QLayout* VLayout = new QVBoxLayout;
	VLayout->addWidget(&(m_topView->getView()));
	VLayout->addWidget(&(m_bottomView->getView()));

	ml->addItem(VLayout);
	ml->addWidget(&(m_editedView->getView()));
	setLayout(ml);

	resizeEvent(0);

}

CLLayoutEditorWnd::~CLLayoutEditorWnd()
{
	destroyNavigator(m_topView);
	destroyNavigator(m_bottomView);
	destroyNavigator(m_editedView);
}

void CLLayoutEditorWnd::closeEvent(QCloseEvent *event)
{

	QMessageBox::StandardButton result = YesNoCancel(this, tr("Layout Editor"), tr("Save changes?"));
	if (result == QMessageBox::Cancel)
	{
		event->ignore();
		return;
	}

	setResult(result);

	destroyNavigator(m_topView);
	destroyNavigator(m_bottomView);
	destroyNavigator(m_editedView);
}

void CLLayoutEditorWnd::resizeEvent ( QResizeEvent * /*event*/)
{
	QSize sz = this->size();
	m_topView->getView().setMaximumWidth(sz.width()/3);
	m_bottomView->getView().setMaximumWidth(sz.width()/3);

	m_topView->getView().setMaximumHeight(sz.height()/2);
	m_bottomView->getView().setMaximumHeight(sz.height()/2);
}

void CLLayoutEditorWnd::destroyNavigator(CLLayoutNavigator*& nav)
{
	if (nav)
	{
		nav->destroy();
		delete nav;
		nav = 0;
	}

}

void CLLayoutEditorWnd::onNewLayoutItemSelected(CLLayoutNavigator* ln, LayoutContent* newl)
{
	if (ln==m_topView)
	{
		m_bottomView->goToNewLayoutContent(newl);
	}
}
