#ifndef MAINWND_H
#define MAINWND_H

#include <QtGui/QMainWindow>
#include "ui_mainwnd.h"
#include "ui/layout_navigator.h"

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
private:
	void closeEvent ( QCloseEvent * event );
private:
	CLLayoutNavigator m_layout;

};

#endif // MAINWND_H
