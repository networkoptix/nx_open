#ifndef QN_DMW_H
#define QN_DMW_H

#include <QtCore/QMargins>
#include <QtWidgets/QWidget>

#ifdef Q_OS_WIN
#   define QN_HAS_DWM
#endif

class QnDwmPrivate;

class QnDwm: public QObject {
    Q_OBJECT

public:
    QnDwm(QWidget *widget);

    virtual ~QnDwm();

    /**
     * \returns                         Whether this API is supported. 
     */
    static bool isSupported();

    /**
     * \returns                         Whether Windows DWM composition is currently enabled on the system.
     */
    bool isCompositionEnabled() const;

    /**
     * Enables blur-behind on a widget.
     * 
     * Note that for aero glass to work, <tt>WA_TranslucentBackground</tt> and
     * <tt>WA_NoSystemBackground</tt> attributes must be set.
     *
     * \param enable                    Whether the blur should be enabled.
     * \returns                         Whether the blur was successfully enabled.
     */
    bool enableBlurBehindWindow(bool enable = true);

    bool disableBlurBehindWindow(bool disable = true) {
        return enableBlurBehindWindow(!disable);
    }

    /**
     * This function controls the rendering of the frame inside a window.
     * Note that passing margins of -1 (the default value) will completely
     * remove the frame from the window.
     *
     * Note that for aero glass to work, <tt>WA_TranslucentBackground</tt> and
     * <tt>WA_NoSystemBackground</tt> attributes must be set.
     *
     * \param margins                   Frame extension margins.
     */
    bool extendFrameIntoClientArea(const QMargins &margins = QMargins(-1, -1, -1, -1));

    /**
     * Note that this function queries frame margins from the system and returned
     * margins may differ from the ones calculated via <tt>QWidget::frameGeometry</tt>.
     * 
     * Also note that window frame includes title bar area.
     *
     * \returns                         Current window's frame margins, negative margins on error.
     */
    QMargins currentFrameMargins() const;

    /**
     * \param margins                   New frame margins for the current window.
     * \returns                         Whether the margins were successfully set.
     */
    bool setCurrentFrameMargins(const QMargins &margins);

    /**
     * Note that returned margins DO NOT include title bar area. 
     *
     * \returns                         Frame margins as dictated by the current desktop theme,
     *                                  Negative margins on error.
     */
    QMargins themeFrameMargins() const;

    /**
     * \returns                         Height of the title bar area as dictated by the 
     *                                  current desktop theme, -1 on error.
     */
    int themeTitleBarHeight() const;

    /**
     * This function is to be called at the end of widget's <tt>event</tt> handler.
     * 
     * \param event                     Event.
     * \returns                         Whether the event was processed.
     */
    bool widgetEvent(QEvent *event);

    /**
     * This function is to be called from widget's <tt>nativeEvent</tt> handler.
     * 
     * \param eventType
     * \param message
     * \param[out] result
     * \returns                         Whether the event was processed.
     */
    bool widgetNativeEvent(const QByteArray &eventType, void *message, long *result);

signals:
    /**
     * This signal is emitted whenever windows composition mode gets enabled or disabled.
     */
    void compositionChanged();

private:
    friend class QnDwmPrivate;

    QnDwmPrivate *d;
};


#endif // QN_DMW_H
