#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

#include <ui/widgets/common/abstract_preferences_widget.h>

#include <utils/common/connective.h>

namespace Ui
{
    class UserAccessRightsResourcesWidget;
}

class QnResourceListModel;

/** Widget for displaying filtered set of accessible resources, for user or user group. */
class QnUserAccessRightsResourcesWidget : public Connective<QnAbstractPreferencesWidget>
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

    QnUserAccessRightsResourcesWidget(Filter filter, QWidget* parent = 0);
    virtual ~QnUserAccessRightsResourcesWidget();

    /** Id of the target user or group. */
    QnUuid targetId() const;

    /** Set if of the user or group. */
    void setTargetId(const QnUuid& id);

    virtual bool hasChanges() const override;
    virtual void loadDataToUi() override;
    virtual void applyChanges() override;

private:
    bool targetIsValid(const QnUuid& id) const;

private:
    QScopedPointer<Ui::UserAccessRightsResourcesWidget> ui;
    const Filter m_filter;
    QnUuid m_targetId;
    QScopedPointer<QnResourceListModel> m_model;
};
