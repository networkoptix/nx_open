#pragma once

#include <core/resource/resource_fwd.h>
#include <ui/dialogs/common/message_box.h>
#include <nx/client/desktop/license/license_helpers.h>

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
        const license::RequestInfo& info,
        QWidget* parent = nullptr);

    virtual ~LicenseDeactivationReason();

    license::RequestInfo info();

private:
    QWidget* createWidget(QPushButton* nextButton);

    static const QStringList reasons();

private:
    license::RequestInfo m_info;
    QSet<QWidget*> m_invalidFields;
};

} // namespace dialogs
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
