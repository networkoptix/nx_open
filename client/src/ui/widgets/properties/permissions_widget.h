#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

namespace Ui
{
    class PermissionsWidget;
}

/** Widget for displaying filtered set of accessible resources, for user or user group. */
class QnPermissionsWidget : public Connective<QnAbstractPreferencesWidget>, public QnWorkbenchContextAware
{
    Q_OBJECT

    typedef Connective<QnAbstractPreferencesWidget> base_type;
public:
    QnPermissionsWidget(QWidget* parent = 0);
    virtual ~QnPermissionsWidget();

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
    QScopedPointer<Ui::PermissionsWidget> ui;
    QnUuid m_targetGroupId;
    QnUserResourcePtr m_targetUser;
};
