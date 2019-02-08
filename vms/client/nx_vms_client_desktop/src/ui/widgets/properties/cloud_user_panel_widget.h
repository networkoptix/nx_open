#pragma once

#include <QtWidgets/QWidget>

#include <nx/vms/client/desktop/common/widgets/panel.h>

namespace Ui { class CloudUserPanelWidget; }
namespace nx::vms::client::desktop { class AbstractAccessor; }

/** Widget for displaying cloud user heading. */
class QnCloudUserPanelWidget: public nx::vms::client::desktop::Panel
{
    Q_OBJECT
    using base_type = nx::vms::client::desktop::Panel;

public:
    QnCloudUserPanelWidget(QWidget* parent = nullptr);
    virtual ~QnCloudUserPanelWidget();

    bool isManageLinkShown() const;
    void setManageLinkShown(bool value);

    QString email() const;
    void setEmail(const QString& value);

    QString fullName() const;
    void setFullName(const QString& value);

    static nx::vms::client::desktop::AbstractAccessor* createIconWidthAccessor();

private:
    void updateManageAccountLink();

private:
    QScopedPointer<Ui::CloudUserPanelWidget> ui;
};
