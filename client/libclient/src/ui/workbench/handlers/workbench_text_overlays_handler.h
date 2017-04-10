#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <business/business_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

class QnResourceWidget;
class QnWorkbenchTextOverlaysHandlerPrivate;
class QnBusinessStringsHelper;

class QnWorkbenchTextOverlaysHandler:
    public Connective<QObject>,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = Connective<QObject>;

public:
    QnWorkbenchTextOverlaysHandler(QObject* parent = nullptr);
    virtual ~QnWorkbenchTextOverlaysHandler();

private:
    void at_businessActionReceived(const QnAbstractBusinessActionPtr& businessAction);
    void at_resourceWidgetAdded(QnResourceWidget* widget);

private:
    Q_DECLARE_PRIVATE(QnWorkbenchTextOverlaysHandler);
    const QScopedPointer<QnWorkbenchTextOverlaysHandlerPrivate> d_ptr;
    QScopedPointer<QnBusinessStringsHelper> m_helper;
};
