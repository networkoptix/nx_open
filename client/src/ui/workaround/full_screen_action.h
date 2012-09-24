#ifndef QN_FULL_SCREEN_ACTION_H
#define QN_FULL_SCREEN_ACTION_H

class QAction;

class QnWorkbenchContext;

/**
 * This class is a workaround for broken OpenGL fullscreen rendering on 
 * X11 with fglrx driver. For this driver we maximize our main window instead
 * of going to fullscreen.
 */
class QnFullScreenAction {
public:
    /**
     * \param context                   Application context to use. Must not be NULL.
     * \returns                         Action to be used as fullscreen action.
     */
    static QAction *get(QnWorkbenchContext *context);

private:
    static bool isAtFglrx(QnWorkbenchContext *context);
};


#endif // QN_FULL_SCREEN_ACTION_H
