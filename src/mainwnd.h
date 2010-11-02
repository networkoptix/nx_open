#ifndef MAINWND_H
#define MAINWND_H

#include <QtGui/QMainWindow>
#include "ui_mainwnd.h"
#include "ui/ui_common.h"

class CLLayoutNavigator;
class QHBoxLayout;
class QVBoxLayout;
class LayoutContent;

//class MainWnd : public QMainWindow
class MainWnd : public QWidget
{
	Q_OBJECT

	

public:
	MainWnd(QWidget *parent = 0, Qt::WFlags flags = 0);
	~MainWnd();
	void toggleFullScreen();
private:

	Ui::MainWndClass ui;

private slots:
	void onItemPressed(QString);
	void onNewLayoutItemSelected(CLLayoutNavigator*, LayoutContent*);

	void goToNomalLayout();
	void goToLayouteditor();

private:
	void closeEvent ( QCloseEvent * event );
	void resizeEvent ( QResizeEvent * event);

	void destroyNavigator(CLLayoutNavigator*& nav);
private:

	CLLayoutNavigator* m_normalView;

	CLLayoutNavigator* m_topView;
	CLLayoutNavigator* m_bottomView;
	CLLayoutNavigator* m_editedView;

	ViewMode mMode;

};

#endif // MAINWND_H
