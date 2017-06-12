#pragma once

#include <QtWidgets/QPushButton>

class QnBusyIndicatorWidget;

class QnBusyIndicatorButton : public QPushButton
{
    Q_OBJECT
    using base_type = QPushButton;

public:
    QnBusyIndicatorButton(QWidget* parent = nullptr);

    QnBusyIndicatorWidget* indicator() const;

    void showIndicator(bool show = true);
    void hideIndicator();
    bool isIndicatorVisible() const;

protected:
    virtual void resizeEvent(QResizeEvent* event) override;
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void changeEvent(QEvent* event) override;

private:
    QnBusyIndicatorWidget* m_indicator;
};
