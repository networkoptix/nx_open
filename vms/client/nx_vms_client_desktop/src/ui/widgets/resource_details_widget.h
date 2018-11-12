#pragma once

#include <core/resource/resource_fwd.h>

#include <nx/vms/client/desktop/common/widgets/panel.h>

#include <utils/common/connective.h>

class QLabel;

namespace nx::vms::client::desktop {

class CameraThumbnailManager;
class AsyncImageWidget;
class TextEditLabel;

} // namespace nx::vms::client::desktop

class QnResourceDetailsWidget: public Connective<nx::vms::client::desktop::Panel>
{
    Q_OBJECT
    using base_type = Connective<nx::vms::client::desktop::Panel>;

public:
    explicit QnResourceDetailsWidget(QWidget* parent = nullptr);
    virtual ~QnResourceDetailsWidget() override;

    QString name() const;
    void setName(const QString& name);

    QString description() const;
    void setDescription(const QString& description);

    QnResourcePtr resource() const;
    void setResource(const QnResourcePtr& resource);

private:
    QScopedPointer<nx::vms::client::desktop::CameraThumbnailManager> m_thumbnailManager;
    nx::vms::client::desktop::AsyncImageWidget* m_preview;
    nx::vms::client::desktop::TextEditLabel* m_nameTextEdit;
    QLabel* m_descriptionLabel;
};
