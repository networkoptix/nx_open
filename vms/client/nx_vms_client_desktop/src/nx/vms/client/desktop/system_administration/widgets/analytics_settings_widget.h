// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/uuid.h>
#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::vms::client::desktop {

class AnalyticsSettingsWidget: public QnAbstractPreferencesWidget, public QnWorkbenchContextAware
{
    Q_OBJECT

    using base_type = QnAbstractPreferencesWidget;

public:
    AnalyticsSettingsWidget(QWidget* parent = nullptr);
    virtual ~AnalyticsSettingsWidget() override;

    virtual void loadDataToUi() override;
    virtual void applyChanges() override;
    virtual void discardChanges() override;
    virtual bool hasChanges() const override;
    virtual bool isNetworkRequestRunning() const override;
    virtual bool activate(const QUrl& url) override;

    bool shouldBeVisible() const;

signals:
    void visibilityUpdateRequested();

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop
