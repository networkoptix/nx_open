// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>

#include <QtCore/QScopedPointer>
#include <QtWidgets/QWidget>

#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui_general_system_administration_widget.h>

#include <nx/vms/client/desktop/common/utils/custom_painted.h>

class QPushButton;

class QnGeneralSystemAdministrationWidget:
    public QnAbstractPreferencesWidget,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef QnAbstractPreferencesWidget base_type;

public:
    QnGeneralSystemAdministrationWidget(QWidget* parent = nullptr);

    virtual void loadDataToUi() override;
    virtual void applyChanges() override;

    virtual bool hasChanges() const override;

    virtual void retranslateUi() override;

protected:
    void setReadOnlyInternal(bool readOnly) override;

private slots:
    void loadSystemName();

private:
    QScopedPointer<Ui::GeneralSystemAdministrationWidget> ui;

    enum Buttons
    {
        kBusinessRulesButton,
        kEventLogButton,
        kCameraListButton,
        kAuditLogButton,
        kBookmarksButton,

        kButtonCount
    };

    using CustomPaintedButton = nx::vms::client::desktop::CustomPainted<QPushButton>;
    std::array<CustomPaintedButton*, kButtonCount> m_buttons;
};
