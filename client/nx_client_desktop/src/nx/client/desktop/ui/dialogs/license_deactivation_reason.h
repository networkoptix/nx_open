#pragma once

#include <core/resource/resource_fwd.h>
#include <ui/dialogs/common/message_box.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace dialogs {

class LicenseDeactivationReason: public QnMessageBox
{
    Q_OBJECT
    using base_type = QnMessageBox;

public:
    LicenseDeactivationReason(
        const QnUserResourcePtr& currentUser,
        QWidget* parent = nullptr);

    virtual ~LicenseDeactivationReason();

    QString name() const;
    QString email() const;
    QStringList reason() const;

private:
    QWidget* createWidget(QPushButton* nextButton);

private:
    QSet<QWidget*> m_invalidFields;
    QString m_name;
    QString m_email;
    QStringList m_reason;
};

} // namespace dialogs
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
