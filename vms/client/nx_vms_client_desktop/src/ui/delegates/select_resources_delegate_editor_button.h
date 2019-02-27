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
    void setResources(const QnUuidSet& resources);

protected:
    virtual void handleButtonClicked() = 0;

signals:
    void commit();

private:
    QnUuidSet m_resources;
};
