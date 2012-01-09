#ifndef QN_DMW_H
#define QN_DMW_H

#include <QWidget>
#include <QMargins>

class QnDwmPrivate;

class QnDwm: public QObject {
    Q_OBJECT;

public:
    QnDwm(QWidget *widget);

    virtual ~QnDwm();

    /**
     * Enables Blur-behind on a Widget.
     *
     * \param enable                    Whether the blur should be enabled.
     */
    bool enableBlurBehindWindow(bool enable);

    /**
     * \returns                         Whether Windows DWM composition is currently enabled on the system.
     */
    bool isCompositionEnabled();

    /**
     * This function controls the rendering of the frame inside a window.
     * Note that passing margins of -1 (the default value) will completely
     * remove the frame from the window.
     *
     * \param margins                   Frame extension margins.
     */
    bool extendFrameIntoClientArea(const QMargins &margins = QMargins(-1, -1, -1, -1));

    /**
     * This function is to be called from widget's <tt>winEvent</tt> handler.
     * 
     * \param message                   Windows message.
     * \param[out] result               Result of message processing.
     * \returns                         Whether the message was processsed.
     */
    bool winEvent(MSG *message, long *result);

    QMargins systemFrameMargins();
    bool setSystemFrameMargins(const QMargins &margins);

    QMargins themeFrameMargins();

    int themeTitleBarHeight();

signals:
    /**
     * This signal is emitted whenever windows composition mode gets enabled or disabled.
     * 
     * \param enabled                   Whether composition mode is enabled.
     */
    void compositionChanged(bool enabled);

protected:
#ifdef Q_OS_WIN
    bool hitTestEvent(MSG *message, long *result);
    bool calcSizeEvent(MSG *message, long *result);
    bool compositionChangedEvent(MSG *message, long *result);
#endif

private:
    QnDwmPrivate *d;
};


#endif // QN_DMW_H
