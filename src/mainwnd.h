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


//class MainWnd : public QMainWindow
class MainWnd : public QWidget
{
	Q_OBJECT

public:
	MainWnd(QWidget *parent = 0, Qt::WFlags flags = 0);
	~MainWnd();

	void toggleFullScreen();

protected slots:
	void onItemPressed(QString name);
private:

	Ui::MainWndClass ui;

	GraphicsView m_videoView;
	QGraphicsScene m_scene;

	SceneLayout m_camlayout;

private:

	void closeEvent ( QCloseEvent * event );

	
};

#endif // MAINWND_H
