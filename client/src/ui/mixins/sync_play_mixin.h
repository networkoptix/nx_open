#ifndef QN_SYNC_PLAY_MIXIN_H
#define QN_SYNC_PLAY_MIXIN_H

#include <QObject>

class QnWorkbenchDisplay;
class QnResourceWidget;
class QnArchiveSyncPlayWrapper;

class QnSyncPlayMixin: public QObject {
    Q_OBJECT;
public:
    QnSyncPlayMixin(QnWorkbenchDisplay *display, QObject *parent = NULL);

protected slots:
    void at_display_widgetAdded(QnResourceWidget *widget);
    void at_display_widgetAboutToBeRemoved(QnResourceWidget *widget);

private:
    QnArchiveSyncPlayWrapper *m_syncPlay;
};

#endif // QN_SYNC_PLAY_MIXIN_H
