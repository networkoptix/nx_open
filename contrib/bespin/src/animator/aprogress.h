#ifndef PROGRESS_ANIMATOR_H
#define PROGRESS_ANIMATOR_H

#include "basic.h"

namespace Animator {

class Progress : public Basic {
    Q_OBJECT
public:
    static bool manage(QWidget *w);
    static void release(QWidget *w);
    static int step(const QWidget *w);
    static float speed();
protected:
    Progress() : Basic() {};
    int _step(const QWidget *widget, long int index = 0) const;
protected slots:
    void timerEvent(QTimerEvent * event);
private:
    Q_DISABLE_COPY(Progress)
};

}

#endif //PROGRESS_ANIMATOR_H
