#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <common/common_globals.h>
#include <ui/workbench/workbench_context_aware.h>
#include <utils/common/connective.h>

class QnMediaResourceWidget;

namespace nx::vms::client::desktop {

/**
 * A base for utility classes to synchronize Right Panel state with Workbench state,
 * for example Motion tab with Motion Search on camera.
 */
class AbstractSearchSynchronizer:
    public Connective<QObject>,
    public QnWorkbenchContextAware
{
    Q_OBJECT

    using base_type = Connective<QObject>;

public:
    AbstractSearchSynchronizer(QnWorkbenchContext* context, QObject* parent = nullptr);
    virtual ~AbstractSearchSynchronizer() override = default;

    bool active() const;
    void setActive(bool value);

    QnMediaResourceWidget* mediaWidget() const;

signals:
    void activeChanged(bool value, QPrivateSignal);
    void mediaWidgetAboutToBeChanged(QnMediaResourceWidget* oldMediaWidget, QPrivateSignal);
    void mediaWidgetChanged(QnMediaResourceWidget* newMediaWidget, QPrivateSignal);

protected:
    void setTimeContentDisplayed(Qn::TimePeriodContent content, bool displayed);
    virtual bool isMediaAccepted(QnMediaResourceWidget* widget) const;

private:
    bool m_active = false;
    QPointer<QnMediaResourceWidget> m_mediaWidget;
};

} // namespace nx::vms::client::desktop
