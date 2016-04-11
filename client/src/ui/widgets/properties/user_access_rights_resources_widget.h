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

class QnUserAccessRightsResourcesWidget : public Connective<QnAbstractPreferencesWidget>
{
    Q_OBJECT

    typedef Connective<QnAbstractPreferencesWidget> base_type;
public:
    QnUserAccessRightsResourcesWidget(QWidget* parent = 0);
    virtual ~QnUserAccessRightsResourcesWidget();

    QSet<QnUuid> checkedResources() const;
    void setCheckedResources(const QSet<QnUuid>& value);

    virtual bool hasChanges() const override;
    virtual void loadDataToUi() override;
    virtual void applyChanges() override;
private:
    QScopedPointer<Ui::UserAccessRightsResourcesWidget> ui;
    QScopedPointer<QnResourceListModel> m_model;
    QSet<QnUuid> m_checkedResources;
};
