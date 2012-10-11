#ifndef QN_ACTION_FACTORIES_H
#define QN_ACTION_FACTORIES_H

#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>

class QAction;

class QnActionFactory: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT;
public:
    QnActionFactory(QObject *parent = NULL):
        QObject(parent),
        QnWorkbenchContextAware(parent)
    {}

    virtual QList<QAction *> newActions(QObject *parent = NULL) {
        Q_UNUSED(parent);

        return QList<QAction *>();
    }
};


class QnOpenCurrentUserLayoutActionFactory: public QnActionFactory {
    Q_OBJECT;
public:
    QnOpenCurrentUserLayoutActionFactory(QObject *parent = NULL): QnActionFactory(parent) {}

    virtual QList<QAction *> newActions(QObject *parent) override;

private slots:
    void at_action_triggered();
};


#endif // QN_ACTION_FACTORIES_H
