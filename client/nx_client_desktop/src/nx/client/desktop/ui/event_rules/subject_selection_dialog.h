#pragma once

#include <functional>

#include <QtCore/QScopedPointer>
#include <QtGui/QValidator>

#include <core/resource/resource_fwd.h>
#include <ui/dialogs/common/session_aware_dialog.h>

class QnUserRolesModel;
class QnUuid;

namespace Ui {
class SubjectSelectionDialog;
} // namespace Ui

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class SubjectSelectionDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnSessionAwareButtonBoxDialog;

public:
    explicit SubjectSelectionDialog(QWidget* parent = nullptr, Qt::WindowFlags windowFlags = 0);
    virtual ~SubjectSelectionDialog() override;

    void showAlert(const QString& text);

    using UserValidator = std::function<QValidator::State(const QnUserResourcePtr&)>;
    void setUserValidator(UserValidator userValidator);

    void setCheckedSubjects(const QSet<QnUuid>& ids);
    QSet<QnUuid> checkedSubjects() const;

private:
    class UserListModel;
    class UserListDelegate;

private:
    QScopedPointer<Ui::SubjectSelectionDialog> ui;
    QnUserRolesModel* m_roles = nullptr;
    UserListModel* m_users = nullptr;
};

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
