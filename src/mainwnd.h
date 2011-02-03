#ifndef MAINWND_H
#define MAINWND_H

#include "ui_mainwnd.h"
#include "ui/ui_common.h"

class CLLayoutNavigator;
class LayoutContent;

//class MainWnd : public QDialog
class MainWnd : public QWidget
{
	Q_OBJECT

	

public:
	MainWnd(QWidget *parent = 0, Qt::WFlags flags = 0);
	~MainWnd();

private:
	void closeEvent ( QCloseEvent * event );
	void destroyNavigator(CLLayoutNavigator*& nav);
private:

	CLLayoutNavigator* m_normalView;

};

#endif // MAINWND_H
