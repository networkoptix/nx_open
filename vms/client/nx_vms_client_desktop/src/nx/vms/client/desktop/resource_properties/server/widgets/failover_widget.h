// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui { class FailoverWidget; }

namespace nx::vms::client::desktop {

class FailoverWidget:
    public QnAbstractPreferencesWidget, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QnAbstractPreferencesWidget;

public:
    FailoverWidget(QWidget* parent = nullptr);
    virtual ~FailoverWidget();

    virtual bool hasChanges() const override;
    virtual void loadDataToUi() override;
    virtual void applyChanges() override;
    virtual void discardChanges() override;
    virtual void retranslateUi() override;
    virtual bool canApplyChanges() const override;
    virtual bool isNetworkRequestRunning() const override;

    QnMediaServerResourcePtr server() const;
    void setServer(const QnMediaServerResourcePtr &server);

protected:
    void setReadOnlyInternal(bool readOnly) override;

private:
    void updateFailoverLabel();
    void updateFailoverSettings();
    void updateMaxCamerasSettings();
    void updateLocationIdSettings();
    void updateReadOnly();

private:
    QScopedPointer<Ui::FailoverWidget> ui;

    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
