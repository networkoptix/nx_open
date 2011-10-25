#include "mainwindow.h"

#include <QtGui/QToolBar>

#include "graphicsview.h"

#include "scene/scenecontroller.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("GV Demo"));

    //GraphicsView *view = new GraphicsView(this);
    QGraphicsView *view = new QGraphicsView(this);

    SceneController *controller = new SceneController(view, this);

    QToolBar *toolBar = new QToolBar(this);
    toolBar->addActions(view->actions());
    addToolBar(Qt::TopToolBarArea, toolBar);

    setCentralWidget(view);

    showMaximized();
}
