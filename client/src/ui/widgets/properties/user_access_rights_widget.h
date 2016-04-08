#pragma once

#include <QtWidgets/QWidget>

#include <nx_ec/data/api_access_rights_data.h>

#include <ui/widgets/common/abstract_preferences_widget.h>

#include <utils/common/connective.h>

namespace Ui
{
    class UserAccessRightsWidget;
}

class QnUserAccessRightsWidget : public Connective<QnAbstractPreferencesWidget>
{
    Q_OBJECT

    typedef Connective<QnAbstractPreferencesWidget> base_type;
public:
    QnUserAccessRightsWidget(QWidget* parent = 0);
    virtual ~QnUserAccessRightsWidget();

    QSet<QnUuid> accessibleResources() const;
    void setAccessibleResources(const QSet<QnUuid>& value);

    virtual bool hasChanges() const override;
    virtual void loadDataToUi() override;
    virtual void applyChanges() override;
private:
    QScopedPointer<Ui::UserAccessRightsWidget> ui;
    QSet<QnUuid> m_data;
};
