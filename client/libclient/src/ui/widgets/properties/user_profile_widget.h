#pragma once

#include <QtWidgets/QWidget>

#include <core/resource/resource_fwd.h>

#include <ui/widgets/common/abstract_preferences_widget.h>
#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

namespace Ui
{
    class UserProfileWidget;
}

class QnUserSettingsModel;
class QnAligner;

class QnUserProfileWidget : public Connective<QnAbstractPreferencesWidget>, public QnWorkbenchContextAware
{
    Q_OBJECT
    typedef Connective<QnAbstractPreferencesWidget> base_type;

public:
    QnUserProfileWidget(QnUserSettingsModel* model, QWidget* parent = 0);
    virtual ~QnUserProfileWidget();

    virtual bool hasChanges() const override;
    virtual void loadDataToUi() override;
    virtual void applyChanges() override;

    virtual bool canApplyChanges() const override;

    void updatePermissionsLabel(const QString& text);

private:
    void updateControlsAccess();
    bool validMode() const;

private:
    QScopedPointer<Ui::UserProfileWidget> ui;
    QnUserSettingsModel* const m_model;
    QString m_newPassword;
    QnAligner* m_aligner;
};
