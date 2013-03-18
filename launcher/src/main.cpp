////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include <QApplication>

#include "launcher_fsm.h"


int main( int argc, char* argv[] )
{
    QApplication app( argc, argv );
    LauncherFSM fsm;
    fsm.start();

    return app.exec();
}
