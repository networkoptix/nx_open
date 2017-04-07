#pragma once

#include <QtCore/QObject>

#include <business/business_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

class QnMediaResourceWidget;

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

    void showTextOverlay(QnMediaResourceWidget* widget, const QnUuid& id,
        const QString& captionHtml, const QString& descriptionHtml, int timeoutMs);
};
