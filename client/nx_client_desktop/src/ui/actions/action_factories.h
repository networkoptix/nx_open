#pragma once

#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>

#include "action_parameters.h"

class QAction;
class QWidget;
class QMenu;

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

    virtual QMenu* newMenu(const QnActionParameters& /*parameters*/, QWidget* /*parentWidget*/)
    {
        return nullptr;
    }
};


class QnOpenCurrentUserLayoutActionFactory: public QnActionFactory {
    Q_OBJECT
public:
    QnOpenCurrentUserLayoutActionFactory(QObject *parent = NULL);
    virtual QList<QAction *> newActions(const QnActionParameters &parameters, QObject *parent) override;
};


class QnPtzPresetsToursActionFactory: public QnActionFactory {
    Q_OBJECT
public:
    QnPtzPresetsToursActionFactory(QObject *parent = NULL): QnActionFactory(parent) {}

    virtual QList<QAction *> newActions(const QnActionParameters &parameters, QObject *parent) override;

private slots:
    void at_action_triggered();
};

class QnEdgeNodeActionFactory: public QnActionFactory {
    Q_OBJECT
public:
    QnEdgeNodeActionFactory(QObject *parent = NULL): QnActionFactory(parent) {}
    virtual QMenu* newMenu(const QnActionParameters &parameters,  QWidget *parentWidget) override;
};

class QnAspectRatioActionFactory: public QnActionFactory {
    Q_OBJECT
public:
    QnAspectRatioActionFactory(QObject *parent = NULL): QnActionFactory(parent) {}
    virtual QList<QAction *> newActions(const QnActionParameters &parameters, QObject *parent) override;
private slots:
    void at_action_triggered(QAction *action);
};
