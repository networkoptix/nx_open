// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSet>
#include <QtCore/QScopedPointer>

#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

#include <nx/utils/uuid.h>

namespace Ui { class AccessibleLayoutsWidget; }

class QnAbstractPermissionsModel;

namespace nx::vms::client::desktop { class ResourceSelectionWidget; }

/**
 * Widget displays filtered set of accessible layouts, for user or user group.
 */
class QnAccessibleLayoutsWidget:
    public QnAbstractPreferencesWidget,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QnAbstractPreferencesWidget;

public:
    QnAccessibleLayoutsWidget(
        QnAbstractPermissionsModel* permissionsModel,
        QWidget* parent);
    virtual ~QnAccessibleLayoutsWidget() override;

    virtual bool hasChanges() const override;
    virtual void loadDataToUi() override;
    virtual void applyChanges() override;

    int layoutsCount() const;
    QSet<QnUuid> checkedLayouts() const;

private:
    QScopedPointer<Ui::AccessibleLayoutsWidget> ui;
    QnAbstractPermissionsModel* const m_permissionsModel;
    nx::vms::client::desktop::ResourceSelectionWidget* m_resourceSelectionWidget = nullptr;
};
