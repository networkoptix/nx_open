#pragma once

#include <QtCore/QItemSelectionModel>

#include <QtWidgets/QWidget>

#include <core/resource_access/resource_access_filter.h>

#include <ui/models/abstract_permissions_model.h>
#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui
{
    class AccessibleResourcesWidget;
}

class QnResourceListModel;
class QnResourceListSortedModel;
class QnAccessibleResourcesModel;

/** Widget for displaying filtered set of accessible resources, for user or user group. */
class QnAccessibleResourcesWidget : public QnAbstractPreferencesWidget,
    public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef QnAbstractPreferencesWidget base_type;
public:
    QnAccessibleResourcesWidget(QnAbstractPermissionsModel* permissionsModel,
        QnResourceAccessFilter::Filter filter, QWidget* parent = 0);
    virtual ~QnAccessibleResourcesWidget();

    virtual bool hasChanges() const override;
    virtual void loadDataToUi() override;
    virtual void applyChanges() override;

    bool isAll() const;
    std::pair<int, int> selected() const;
    QnResourceAccessFilter::Filter filter() const;

    QSet<QnUuid> checkedResources() const;

    bool resourcePassFilter(const QnResourcePtr& resource) const;
    static bool resourcePassFilter(const QnResourcePtr& resource,
        const QnUserResourcePtr& currentUser, QnResourceAccessFilter::Filter filter);

private:
    void initControlsModel();
    void initResourcesModel();
    void initSortFilterModel();

    void at_itemViewKeyPress(QObject* watched, QEvent* event);

    void updateThumbnail(const QModelIndex& index = QModelIndex());

    void handleResourceAdded(const QnResourcePtr& resource);
    void refreshModel();

signals:
    void controlsChanged(bool useAll);

private:
    QScopedPointer<Ui::AccessibleResourcesWidget> ui;
    QnAbstractPermissionsModel* const m_permissionsModel;
    const QnResourceAccessFilter::Filter m_filter;
    const bool m_controlsVisible;   /*< Should the controls widget be visible and active. */

    QnResourceListModel* m_resourcesModel;
    QnResourceListModel* m_controlsModel;    /*< Workaround to make controls checkboxes look totally like elements in list. */
    QnResourceListSortedModel* m_sortFilterModel;

    QnAccessibleResourcesModel* m_accessibleResourcesModel;
};
