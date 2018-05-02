#pragma once

#include <QtWidgets/QLabel>

class QnWordWrappedLabel: public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(Qt::TextFormat textFormat READ textFormat WRITE setTextFormat)
    Q_PROPERTY(bool openExternalLinks READ openExternalLinks WRITE setOpenExternalLinks)

    using base_type = QWidget;
public:
    QnWordWrappedLabel(QWidget* parent = 0);

    QLabel* label() const;

    virtual QSize sizeHint() const override;
    virtual QSize minimumSizeHint() const override;

    QString text() const;
    void setText(const QString& value);

    Qt::TextFormat textFormat() const;
    void setTextFormat(Qt::TextFormat value);

    int approximateLines() const;
    void setApproximateLines(int value);

    bool openExternalLinks() const;
    void setOpenExternalLinks(bool value);

signals:
    void linkActivated(const QString& value);

protected:
    virtual void showEvent(QShowEvent* event) override;
    virtual void resizeEvent(QResizeEvent* event) override;

private:
    void ensureInitialized();

private:
    QLabel* m_label = nullptr;
    bool m_initialized = false;
    int m_approximateLines = 2;
};
