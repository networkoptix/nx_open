// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSet>
#include <QtCore/QScopedPointer>

#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

#include <nx/utils/uuid.h>

namespace Ui { class AccessibleMediaWidget; }

class QnAbstractPermissionsModel;

namespace nx::vms::client::desktop { class AccessibleMediaSelectionWidget; }
namespace nx::vms::client::desktop { class AccessibleMediaViewHeaderWidget; }

/**
 * Widget displays filtered set of accessible media resources, for user or user group.
 */
class QnAccessibleMediaWidget:
    public QnAbstractPreferencesWidget,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QnAbstractPreferencesWidget;

public:
    QnAccessibleMediaWidget(
        QnAbstractPermissionsModel* permissionsModel,
        QWidget* parent);
    virtual ~QnAccessibleMediaWidget() override;

    virtual bool hasChanges() const override;
    virtual void loadDataToUi() override;
    virtual void applyChanges() override;

    bool allCamerasItemChecked() const;
    int resourcesCount() const;
    QSet<QnUuid> checkedResources() const;

private:
    QScopedPointer<Ui::AccessibleMediaWidget> ui;
    QnAbstractPermissionsModel* const m_permissionsModel;
    nx::vms::client::desktop::AccessibleMediaSelectionWidget* m_mediaSelectionWidget = nullptr;
    nx::vms::client::desktop::AccessibleMediaViewHeaderWidget* m_headerWidget = nullptr;
};
