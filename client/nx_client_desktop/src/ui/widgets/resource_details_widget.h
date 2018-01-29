#pragma once

#include <core/resource/resource_fwd.h>

#include <ui/widgets/common/panel.h>

#include <utils/common/connective.h>

class QLabel;
class QnTextEditLabel;
class QnCameraThumbnailManager;
namespace nx { namespace client { namespace desktop { class AsyncImageWidget; } } }

class QnResourceDetailsWidget : public Connective<QnPanel>
{
    Q_OBJECT
    typedef Connective<QnPanel> base_type;

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
    QScopedPointer<QnCameraThumbnailManager> m_thumbnailManager;
    nx::client::desktop::AsyncImageWidget* m_preview;
    QnTextEditLabel* m_nameTextEdit;
    QLabel* m_descriptionLabel;
};
