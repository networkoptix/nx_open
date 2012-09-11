#ifndef APP_FOCUS_LISTENER_H
#define APP_FOCUS_LISTENER_H

#include <QObject>

class QnAppFocusListener: public QObject{
    Q_OBJECT

public:
    void hideLauncher();
    void restoreLauncher();

protected:
    bool eventFilter(QObject *, QEvent *);

};

#endif // APP_FOCUS_LISTENER_H
