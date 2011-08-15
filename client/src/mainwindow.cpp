#include "mainwindow.h"

#include <QtGui/QToolBar>

#include "graphicsview.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("VMS Client"));

    GraphicsView *view = new GraphicsView(this);

    QToolBar *toolBar = new QToolBar(this);
    toolBar->addActions(view->actions());
    addToolBar(Qt::TopToolBarArea, toolBar);

    setCentralWidget(view);

    showMaximized();
}
