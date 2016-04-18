#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

namespace Ui
{
    class AccessibleResourcesWidget;
}

class QnResourceListModel;

/** Widget for displaying filtered set of accessible resources, for user or user group. */
class QnAccessibleResourcesWidget : public Connective<QnAbstractPreferencesWidget>, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef Connective<QnAbstractPreferencesWidget> base_type;
public:
    enum Filter
    {
        CamerasFilter,
        LayoutsFilter,
        ServersFilter
    };

    QnAccessibleResourcesWidget(Filter filter, QWidget* parent = 0);
    virtual ~QnAccessibleResourcesWidget();

    /** Id of the target . */
    QnUuid targetGroupId() const;
    /** Set if of the group. */
    void setTargetGroupId(const QnUuid& id);

    QnUserResourcePtr targetUser() const;
    void setTargetUser(const QnUserResourcePtr& user);

    virtual bool hasChanges() const override;
    virtual void loadDataToUi() override;
    virtual void applyChanges() override;

private:
    bool targetIsValid() const;
    QnUuid targetId() const;

private:
    QScopedPointer<Ui::AccessibleResourcesWidget> ui;
    const Filter m_filter;
    QnUuid m_targetGroupId;
    QnUserResourcePtr m_targetUser;
    QScopedPointer<QnResourceListModel> m_model;
};
