#ifndef QN_RENDER_WATCHER_H
#define QN_RENDER_WATCHER_H

#include <QObject>
#include <QHash>
#include "abstractrenderer.h"

class QnRenderWatcher: public QObject {
    Q_OBJECT;
public:
    QnRenderWatcher(QObject *parent = NULL);

    void registerRenderer(CLAbstractRenderer *renderer, QObject *lifetime);

    void unregisterRenderer(CLAbstractRenderer *renderer);

    void startDisplay();

    void finishDisplay();

signals:
    void displayingStateChanged(CLAbstractRenderer *renderer, bool displaying);

private slots:
    void at_lifetime_destroyed();

private:
    struct Info {
        Info(): displayCounter(0), displaying(false), lifetime(NULL) {}

        Info(int displayCounter, bool displaying, QObject *lifetime): displayCounter(displayCounter), displaying(displaying), lifetime(lifetime) {}

        int displayCounter;
        bool displaying;
        QObject *lifetime;
    };

    QHash<CLAbstractRenderer *, Info> m_infoByRenderer;
    QHash<QObject *, CLAbstractRenderer *> m_rendererByLifetime;
};

#endif // QN_RENDER_WATCHER_H
