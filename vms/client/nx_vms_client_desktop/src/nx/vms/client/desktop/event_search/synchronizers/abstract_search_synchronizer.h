// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>

#include <common/common_globals.h>
#include <ui/workbench/workbench_context_aware.h>

class QnMediaResourceWidget;

namespace nx::vms::client::desktop {

class CommonObjectSearchSetup;

/**
 * A base for utility classes to synchronize Right Panel state with Workbench state,
 * for example Motion tab with Motion Search on camera.
 */
class AbstractSearchSynchronizer:
    public QObject,
    public WindowContextAware
{
    Q_OBJECT

    using base_type = QObject;

public:
    AbstractSearchSynchronizer(
        WindowContext* context,
        CommonObjectSearchSetup* commonSetup,
        QObject* parent = nullptr);
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
    CommonObjectSearchSetup* commonSetup();

private:
    bool m_active = false;
    QPointer<QnMediaResourceWidget> m_mediaWidget;
    const QPointer<CommonObjectSearchSetup> m_commonSetup;
};

} // namespace nx::vms::client::desktop
