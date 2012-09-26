#ifndef APP_FOCUS_LISTENER_H
#define APP_FOCUS_LISTENER_H

#include <QObject>

// TODO: #GDM doxydocs, motivation - what is this class for.
// 
// Class naming:
// I read 'application focus listener' -> I think 'Cool! I can use this class to listen to application focus changes',
// but the truth is that I cannot and this class actually implements an X11 workaround.
class QnAppFocusListener: public QObject{
    Q_OBJECT

public:
    QnAppFocusListener();
    virtual ~QnAppFocusListener();

    void hideLauncher();
    void restoreLauncher();

protected:
    bool eventFilter(QObject *, QEvent *);

};

#endif // APP_FOCUS_LISTENER_H
