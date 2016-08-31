#pragma once

#include <core/resource/resource_fwd.h>

#include <ui/widgets/common/panel.h>

#include <utils/common/connective.h>

class QLabel;
class QnTextEditLabel;
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

    const QnResourcePtr& targetResource() const;
    void setTargetResource(const QnResourcePtr& target);

private:
    QnResourcePreviewWidget* m_preview;
    QnTextEditLabel* m_nameTextEdit;
    QLabel* m_descriptionLabel;
};
