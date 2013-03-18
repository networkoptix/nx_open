////////////////////////////////////////////////////////////
// 13 mar 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef USER_CHOOSES_WHAT_TO_LAUNCH_H
#define USER_CHOOSES_WHAT_TO_LAUNCH_H

#include <QState>


class UserChoosesWhatToLaunch
:
    public QState
{
    Q_OBJECT

public:
    UserChoosesWhatToLaunch( QState* const parent );

signals:
    void cancelled();
};

#endif  //USER_CHOOSES_WHAT_TO_LAUNCH_H
