#ifndef MAINWND_H
#define MAINWND_H

#include <QtGui/QMainWindow>
#include "ui_mainwnd.h"
#include "./ui/graphicsview.h"
#include <QList>
#include "streamreader/streamreader.h"
#include "device/device.h"
#include "ui/video_cam_layout/videocamlayout.h"



#define CL_MAX_CAM_SUPPORTED 400

class QTimer;
class CLVideoCamera;
class CLVideoWindow;
class VideoWindow;
class CLDevice;

typedef QList<VideoWindow*> CLVideoWindowsList;
typedef QList<CLVideoCamera*> CLVideoCamsList;



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

	GraphicsView m_videoView;
	QGraphicsScene m_scene;


	QTimer *m_timer;
	QTimer *m_videotimer;



	CLVideoCamera* m_selectedcCam;

	CLVideoWindowsList m_videoWindows;
	CLVideoCamsList m_cams;

	VideoCamerasLayout m_camlayout;

	int m_scene_right;
	int m_scene_bottom;


private slots:
	void onTimer();
	void onVideoTimer();

private:

	void closeEvent ( QCloseEvent * event );


	void onFirstSceneAppearance();

	void onNewDevices_helper(CLDeviceList devices);
	void onNewDevice(CLDevice* device);

	
};

#endif // MAINWND_H
