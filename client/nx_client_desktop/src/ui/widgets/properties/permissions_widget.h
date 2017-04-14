#pragma once

#include <QtWidgets/QWidget>

#include <common/common_globals.h>

#include <ui/widgets/common/abstract_preferences_widget.h>

class QnAbstractPermissionsModel;

namespace Ui
{
    class PermissionsWidget;
}

/** Widget for displaying filtered set of accessible resources, for user or user group. */
class QnPermissionsWidget : public QnAbstractPreferencesWidget
{
    Q_OBJECT

    typedef QnAbstractPreferencesWidget base_type;
public:
    QnPermissionsWidget(QnAbstractPermissionsModel* permissionsModel, QWidget* parent = 0);
    virtual ~QnPermissionsWidget();

    virtual bool hasChanges() const override;
    virtual void loadDataToUi() override;
    virtual void applyChanges() override;

    Qn::GlobalPermissions selectedPermissions() const;

private:
    void updateDependentPermissions();
    void addCheckBox(
        Qn::GlobalPermission permission,
        const QString& text,
        const QString& description = QString());

private:
    QScopedPointer<Ui::PermissionsWidget> ui;

    QnAbstractPermissionsModel* const m_permissionsModel;

    QList<QCheckBox*> m_checkboxes;
};
