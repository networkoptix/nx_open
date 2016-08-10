#pragma once

#include <ui/widgets/common/panel.h>
#include <utils/common/connective.h>

class QLabel;
class QPlainTextEdit;
class QnResourcePreviewWidget;

class QnResourceDetailsWidget : public Connective<QnPanel>
{
    Q_OBJECT
    typedef Connective<QnPanel> base_type;

public:
    explicit QnResourceDetailsWidget(QWidget* parent = nullptr);

    QString name() const;
    void setName(const QString& name);

    QString description() const;
    void setDescription(const QString& description);

    QnUuid targetResource() const;
    void setTargetResource(const QnUuid& target);

#if 0
    QSize thumbnailSize() const;
    void setThumbnailSize(const QSize& size);
#endif

private:
    QnResourcePreviewWidget* m_preview;
    QPlainTextEdit* m_nameTextEdit;
    QLabel* m_descriptionLabel;
};
