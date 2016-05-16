#pragma once

#include <QtWidgets/QWidget>

#include <core/resource_management/resource_access_filter.h>

#include <ui/models/abstract_permissions_model.h>
#include <ui/widgets/common/abstract_preferences_widget.h>

namespace Ui
{
    class AccessibleResourcesWidget;
}

class QnResourceListModel;

/** Widget for displaying filtered set of accessible resources, for user or user group. */
class QnAccessibleResourcesWidget : public QnAbstractPreferencesWidget
{
    Q_OBJECT

    typedef QnAbstractPreferencesWidget base_type;
public:
    QnAccessibleResourcesWidget(QnAbstractPermissionsModel* permissionsModel, QnResourceAccessFilter::Filter filter, QWidget* parent = 0);
    virtual ~QnAccessibleResourcesWidget();

    virtual bool hasChanges() const override;
    virtual void loadDataToUi() override;
    virtual void applyChanges() override;

private:
    QScopedPointer<Ui::AccessibleResourcesWidget> ui;
    QnAbstractPermissionsModel* const m_permissionsModel;
    const QnResourceAccessFilter::Filter m_filter;

    QScopedPointer<QnResourceListModel> m_resourcesModel;
    QScopedPointer<QnResourceListModel> m_controlsModel;    /*< Workaround to make controls checkboxes look totally like elements in list. */
    QScopedPointer<QSortFilterProxyModel> m_viewModel;
};
