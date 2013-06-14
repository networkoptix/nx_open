////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef CRITICAL_ERROR_MESSAGE_H
#define CRITICAL_ERROR_MESSAGE_H

#include <QState>


class CriticalErrorMessage
:
    public QState
{
    Q_OBJECT

public:
    CriticalErrorMessage( QState* const parent );

signals:
    void ok();
};

#endif  //CRITICAL_ERROR_MESSAGE_H
