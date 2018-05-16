#pragma once

#include <core/resource/resource_fwd.h>

#include <nx/client/desktop/common/widgets/panel.h>

#include <utils/common/connective.h>

class QLabel;
namespace nx { namespace client { namespace desktop { class CameraThumbnailManager; }}}
namespace nx { namespace client { namespace desktop { class AsyncImageWidget; }}}
namespace nx { namespace client { namespace desktop { class TextEditLabel; }}}

class QnResourceDetailsWidget: public Connective<nx::client::desktop::Panel>
{
    Q_OBJECT
    using base_type = Connective<nx::client::desktop::Panel>;

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
    QScopedPointer<nx::client::desktop::CameraThumbnailManager> m_thumbnailManager;
    nx::client::desktop::AsyncImageWidget* m_preview;
    nx::client::desktop::TextEditLabel* m_nameTextEdit;
    QLabel* m_descriptionLabel;
};
