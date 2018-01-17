#pragma once

#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>

#include <ui/dialogs/common/button_box_dialog.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
class MediaFileSettingsDialog;
}

class QnImageProvider;

namespace nx {
namespace client {
namespace desktop {

class MediaFileSettingsDialog:
    public QnButtonBoxDialog,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QnButtonBoxDialog;

public:
    explicit MediaFileSettingsDialog(QWidget* parent);
    virtual ~MediaFileSettingsDialog();

    void updateFromResource(const QnMediaResourcePtr& resource);
    void submitToResource(const QnMediaResourcePtr& resource);

private slots:
    void paramsChanged();

private:
    QScopedPointer<Ui::MediaFileSettingsDialog> ui;

    bool m_updating;
    QnMediaResourcePtr m_resource;
    QScopedPointer<QnImageProvider> m_imageProvider;
};

} // namespace desktop
} // namespace client
} // namespace nx
