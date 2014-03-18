#ifndef QN_ACTION_FACTORIES_H
#define QN_ACTION_FACTORIES_H

#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>

#include "action_parameters.h"

class QAction;

class QnActionFactory: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT
public:
    QnActionFactory(QObject *parent = NULL):
        QObject(parent),
        QnWorkbenchContextAware(parent)
    {}

    virtual QList<QAction *> newActions(const QnActionParameters &parameters, QObject *parent = NULL) {
        Q_UNUSED(parameters);
        Q_UNUSED(parent);

        return QList<QAction *>();
    }
};


class QnOpenCurrentUserLayoutActionFactory: public QnActionFactory {
    Q_OBJECT
public:
    QnOpenCurrentUserLayoutActionFactory(QObject *parent = NULL): QnActionFactory(parent) {}

    virtual QList<QAction *> newActions(const QnActionParameters &parameters, QObject *parent) override;

private slots:
    void at_action_triggered();
};


class QnPtzPresetsToursActionFactory: public QnActionFactory {
    Q_OBJECT
public:
    QnPtzPresetsToursActionFactory(QObject *parent = NULL): QnActionFactory(parent) {}

    virtual QList<QAction *> newActions(const QnActionParameters &parameters, QObject *parent) override;

private slots:
    void at_action_triggered();
};

#endif // QN_ACTION_FACTORIES_H
