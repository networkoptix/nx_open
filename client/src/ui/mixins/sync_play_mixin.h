#ifndef QN_SYNC_PLAY_MIXIN_H
#define QN_SYNC_PLAY_MIXIN_H

#include <QObject>
#include <QWeakPointer>

class QnWorkbenchDisplay;
class QnResourceWidget;
class QnArchiveSyncPlayWrapper;

class CLAbstractRenderer;

class QnSyncPlayMixin: public QObject {
    Q_OBJECT;
public:
    QnSyncPlayMixin(QnWorkbenchDisplay *display, QObject *parent = NULL);

protected slots:
    void at_display_widgetAdded(QnResourceWidget *widget);
    void at_display_widgetAboutToBeRemoved(QnResourceWidget *widget);
    void at_renderWatcher_displayingStateChanged(CLAbstractRenderer *renderer, bool displaying);

private:
    QnArchiveSyncPlayWrapper *m_syncPlay;
    QWeakPointer<QnWorkbenchDisplay> m_display;
};

#endif // QN_SYNC_PLAY_MIXIN_H
