#ifndef MAINWND_H
#define MAINWND_H

#include <QtGui/QMainWindow>
#include "ui_mainwnd.h"

class CLLayoutNavigator;
class QHBoxLayout;
class QVBoxLayout;

//class MainWnd : public QMainWindow
class MainWnd : public QWidget
{
	Q_OBJECT

	enum ViewMode {ZERRO, NORMAL, LAYOUTEDITOR};

public:
	MainWnd(QWidget *parent = 0, Qt::WFlags flags = 0);
	~MainWnd();
	void toggleFullScreen();
private:

	Ui::MainWndClass ui;
private:
	void closeEvent ( QCloseEvent * event );

	void goToNomalLayout();
	void goToLayouteditor();


	void destroyNavigator(CLLayoutNavigator*& nav);
private:

	CLLayoutNavigator* m_normalView;

	CLLayoutNavigator* m_topView;
	CLLayoutNavigator* m_bottomView;
	CLLayoutNavigator* m_editedView;

	ViewMode mMode;

};

#endif // MAINWND_H
