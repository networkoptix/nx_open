#pragma once

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

class QnWorkbenchWebPageHandler : public Connective<QObject>, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef Connective<QObject> base_type;

public:
    QnWorkbenchWebPageHandler(QObject *parent = 0);
    virtual ~QnWorkbenchWebPageHandler();

private:
    void at_newWebPageAction_triggered();

};
