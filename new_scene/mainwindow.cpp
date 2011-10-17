#include "mainwindow.h"

#include <QtGui/QToolBar>

#include "graphicsview.h"

#include "instruments/instrumentmanager.h"
#include "instruments/handscrollinstrument.h"
#include "instruments/wheelzoominstrument.h"
#include "instruments/rubberbandinstrument.h"
#include "instruments/draginstrument.h"
#include "instruments/contextmenuinstrument.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("GV Demo"));

    GraphicsView *view = new GraphicsView(this);

    InstrumentManager *manager = new InstrumentManager(this);
    manager->registerScene(view->scene());
    manager->registerView(view);

    manager->installInstrument(new WheelZoomInstrument(this));
    manager->installInstrument(new RubberBandInstrument(this));
    manager->installInstrument(new HandScrollInstrument(this));
    //manager->installInstrument(new DragInstrument(this));
    manager->installInstrument(new ContextMenuInstrument(this));

    QToolBar *toolBar = new QToolBar(this);
    toolBar->addActions(view->actions());
    addToolBar(Qt::TopToolBarArea, toolBar);

    setCentralWidget(view);

    showMaximized();
}
