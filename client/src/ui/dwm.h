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
     * \returns                         Whether this API is supported. 
     */
    bool isSupported() const;

    /**
     * \returns                         Whether Windows DWM composition is currently enabled on the system.
     */
    bool isCompositionEnabled() const;

    bool enableTransparentErasing(bool enable = true);

    bool disableTransparentErasing(bool disable = true) {
        return enableTransparentErasing(!disable);
    }

    bool setNonErasableContentMargins(const QMargins &margins);


    bool enableSystemWindowPainting(bool enable = true);

    bool disableSystemWindowPainting(bool disable = true) {
        return enableSystemWindowPainting(!disable);
    }

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
     * Enables or disables emulation of window frame in client area.
     * Emulation is performed by feeding the proper frame sections to the OS even
     * when frame section is queried for client area.
     * 
     * \param enable                    Whether window frame should be emulated.
     * \returns                         Whether window frame emulation mode was successfully changed.
     */
    bool enableFrameEmulation(bool enable = true);

    bool disableFrameEmulation(bool disable = true) {
        return enableFrameEmulation(!disable);
    }

    /**
     * \returns                         Whether window frame is emulated.
     */
    bool isFrameEmulated() const;

    /**
     * Sets emulated frame margins. Note that they DO NOT include title bar area, 
     * which can be tuned separately.
     *
     * The effect of setting emulated margins is that it will become possible
     * to resize the window even if it has no "real" frame.
     *
     * \param margins                   New emulated frame margins. 
     */
    void setEmulatedFrameMargins(const QMargins &margins);

    /**
     * \returns                         Currently set emulated frame margins.
     */
    QMargins emulatedFrameMargins() const;

    /**
     * \param height                    New emulated title bar height. 
     */
    void setEmulatedTitleBarHeight(int height);

    /**
     * \returns                         Currently set emulated title bar height.
     */
    int emulatedTitleBarHeight() const;

    /**
     * Enables or disable processing of double clicks on title bar.
     * When enabled, double clicks can be tracked by connecting to the 
     * <tt>titleBarDoubleClicked</tt> signal.
     * 
     * \param enable                    Whether double click processing is to be enabled.
     * \returns                         Whether double click processing mode was successfully changed.
     */
    bool enableDoubleClickProcessing(bool enable = true);

    bool disableDoubleClickProcessing(bool disable = true) {
        return enableDoubleClickProcessing(!disable);
    }

    /**
     * Enables or disables window dragging by clicking on a title bar.
     * 
     * \param enable                    Whether dragging is to be enabled.
     * \returns                         Whether dragging mode was successfully changed.
     */
    bool enableTitleBarDrag(bool enable = true);

    bool disableTitleBarDrag(bool disable = true) {
        return enableTitleBarDrag(!disable);
    }

#ifdef Q_OS_WIN
    /**
     * This function is to be called from widget's <tt>winEvent</tt> handler.
     * 
     * \param message                   Windows message.
     * \param[out] result               Result of message processing.
     * \returns                         Whether the message was processsed.
     */
    bool winEvent(MSG *message, long *result);
#endif

signals:
    /**
     * This signal is emitted whenever windows composition mode gets enabled or disabled.
     * 
     * \param enabled                   Whether composition mode is enabled.
     */
    void compositionChanged(bool enabled);

    /**
     * This signal is emitted whenever the user double-clicks on widget title bar.
     * Double click processing must be enabled.
     */
    void titleBarDoubleClicked();

protected:
#ifdef Q_OS_WIN
    bool hitTestEvent(MSG *message, long *result);
    bool calcSizeEvent(MSG *message, long *result);
    bool compositionChangedEvent(MSG *message, long *result);
    bool activateEvent(MSG *message, long *result);
    bool ncPaintEvent(MSG *message, long *result);
    bool ncLeftButtonDoubleClickEvent(MSG *message, long *result);
    bool sysCommandEvent(MSG *message, long *result);
    bool eraseBackground(MSG *message, long *result);
#endif

private:
    QnDwmPrivate *d;
};


#endif // QN_DMW_H
