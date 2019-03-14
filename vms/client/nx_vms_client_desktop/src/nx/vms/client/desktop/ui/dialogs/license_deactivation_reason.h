#pragma once

#include <core/resource/resource_fwd.h>
#include <ui/dialogs/common/message_box.h>
#include <nx/vms/client/desktop/license/license_helpers.h>
#include <ui/dialogs/common/session_aware_dialog.h>

namespace nx::vms::client::desktop {
namespace ui {
namespace dialogs {


class LicenseDeactivationReason: public QnSessionAwareMessageBox
{
    Q_OBJECT
    using base_type = QnSessionAwareMessageBox;

public:
    LicenseDeactivationReason(
        const license::RequestInfo& info,
        QWidget* parent = nullptr);

    virtual ~LicenseDeactivationReason();

    license::RequestInfo info();

private:
    QWidget* createWidget(QPushButton* nextButton);

    static QStringList reasons();

private:
    license::RequestInfo m_info;
    QSet<QWidget*> m_invalidFields;
};

} // namespace dialogs
} // namespace ui
} // namespace nx::vms::client::desktop
