#pragma once

#include <QtWidgets/QWidget>

class QLabel;

class QnAlertBar: public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)

    using base_type = QWidget;

public:
    QnAlertBar(QWidget* parent = nullptr);

    QString text() const;
    void setText(const QString& text);

    bool reservedSpace() const;
    void setReservedSpace(bool reservedSpace);

private:
    void updateVisibility();

private:
    QLabel* m_label;
    bool m_reservedSpace;
};
