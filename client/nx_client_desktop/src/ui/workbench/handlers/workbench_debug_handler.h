#pragma once

#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>

class QnDebugWebPageHandler: public QObject
{
    Q_OBJECT
public:
    QnDebugWebPageHandler(QObject* parent): QObject(parent){}
public slots:
    Q_INVOKABLE void c2pplayback(const QString& cameras, int timestamp);
};

class QnWorkbenchDebugHandler : public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef QObject base_type;

public:
    explicit QnWorkbenchDebugHandler(QObject *parent = 0);

private slots:
    void at_debugControlPanelAction_triggered();
    void at_debugIncrementCounterAction_triggered();
    void at_debugDecrementCounterAction_triggered();
};

