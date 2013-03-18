////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef DOWNLOAD_SETUP_H
#define DOWNLOAD_SETUP_H

#include <QState>


class DownloadSetup
:
    public QState
{
    Q_OBJECT

public:
    DownloadSetup( QState* const parent );

signals:
    void ok();
};

#endif  //DOWNLOAD_SETUP_H
