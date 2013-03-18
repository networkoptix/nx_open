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

public slots:
    void prepareResultMessage();

signals:
    void succeeded();
};

#endif  //DOWNLOADING_H
