#ifndef MAINWND_H
#define MAINWND_H

#include <QtGui/QMainWindow>
#include "ui_mainwnd.h"
#include "graphicsview.h"
#include <QList>
#include "streamreader/streamreader.h"
#include "device/device.h"



#define CL_MAX_CAM_SUPPORTED 500

class QTimer;
class CLVideoCamera;
class CLVideoWindow;
class VideoWindow;
class CLDevice;

typedef QList<VideoWindow*> CLVideoWindowsList;
typedef QList<CLVideoCamera*> CLVideoCamsList;



//class MainWnd : public QMainWindow, public CLDeviceRestartHadler
class MainWnd : public QWidget, public CLDeviceRestartHadler
{
	Q_OBJECT

public:
	MainWnd(QWidget *parent = 0, Qt::WFlags flags = 0);
	~MainWnd();

private:
	Ui::MainWndClass ui;

	GraphicsView m_videoView;
	QGraphicsScene m_scene;


	QTimer *m_timer;
	QTimer *m_videotimer;


	bool m_position_busy[CL_MAX_CAM_SUPPORTED];

	CLVideoCamera* m_selectedcCam;

	CLVideoWindowsList m_videoWindows;
	CLVideoCamsList m_cams;



private slots:
	void onTimer();
	void onVideoTimer();
private:


	void onDeviceRestarted(CLStreamreader* reader, CLRestartHadlerInfo info);

	void getXY_by_position(int position, int unit_size, int&x, int& y);
	int next_available_pos() const;

	void onNewDevices_helper(CLDeviceList devices);
	void onNewDevice(CLDevice* device);

	CLVideoCamera* getCamByWnd(CLVideoWindow* wnd);
	bool isSelectedCamStillExists() const;


};

#endif // MAINWND_H
