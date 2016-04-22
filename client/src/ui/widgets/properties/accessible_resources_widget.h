#pragma once

#include <QtWidgets/QWidget>

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
    QnAccessibleResourcesWidget(QnAbstractPermissionsModel* permissionsModel, QnAbstractPermissionsModel::Filter filter, QWidget* parent = 0);
    virtual ~QnAccessibleResourcesWidget();

    virtual bool hasChanges() const override;
    virtual void loadDataToUi() override;
    virtual void applyChanges() override;

private:
    QScopedPointer<Ui::AccessibleResourcesWidget> ui;
    QnAbstractPermissionsModel* const m_permissionsModel;
    const QnAbstractPermissionsModel::Filter m_filter;

    QScopedPointer<QnResourceListModel> m_resourcesModel;
};
