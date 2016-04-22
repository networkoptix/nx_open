#pragma once

#include <QtWidgets/QWidget>

#include <common/common_globals.h>

#include <ui/widgets/common/abstract_preferences_widget.h>

class QnAbstractPermissionsDelegate;

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
    QnPermissionsWidget(QnAbstractPermissionsDelegate* delegate, QWidget* parent = 0);
    virtual ~QnPermissionsWidget();

    virtual bool hasChanges() const override;
    virtual void loadDataToUi() override;
    virtual void applyChanges() override;

    Qn::GlobalPermissions rawPermissions() const;
private:
    QScopedPointer<Ui::PermissionsWidget> ui;

    QnAbstractPermissionsDelegate* const m_delegate;

    QList<QCheckBox*> m_checkboxes;
};
