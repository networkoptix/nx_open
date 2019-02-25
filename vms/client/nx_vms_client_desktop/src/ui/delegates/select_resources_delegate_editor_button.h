#pragma once

#include <QtWidgets/QPushButton>

class QnSelectResourcesDialogButton : public QPushButton
{
    Q_OBJECT
    using base_type = QPushButton;

public:
    explicit QnSelectResourcesDialogButton(QWidget* parent = nullptr);
    virtual ~QnSelectResourcesDialogButton() override;

    QnUuidSet getResources() const;
    void setResources(QnUuidSet resources);

signals:
    void commit();

private:
    QnUuidSet m_resources;
};
