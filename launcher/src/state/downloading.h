////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef DOWNLOADING_H
#define DOWNLOADING_H

#include <QState>


class Downloading
:
    public QState
{
    Q_OBJECT

public:
    Downloading( QState* const parent );
};

#endif  //DOWNLOADING_H
