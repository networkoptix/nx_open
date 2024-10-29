// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
    QnWordWrappedLabel(QWidget* parent = nullptr);

    QLabel* label() const;

    virtual QSize sizeHint() const override;
    virtual QSize minimumSizeHint() const override;
    virtual int heightForWidth(int width) const override;
    virtual bool hasHeightForWidth() const override;

    QString text() const;
    void setText(const QString& value);

    Qt::TextFormat textFormat() const;
    void setTextFormat(Qt::TextFormat value);

    bool openExternalLinks() const;
    void setOpenExternalLinks(bool value);

    /** Overload to request correct value from label directly. */
    QPalette::ColorRole foregroundRole() const;

    /** Overload to pass correct value to label directly. */
    void setForegroundRole(QPalette::ColorRole role);

signals:
    void linkActivated(const QString& value);

protected:
    virtual void showEvent(QShowEvent* event) override;

private:
    void ensureInitialized();

private:
    QLabel* m_label = nullptr;
    bool m_initialized = false;
    const int m_rowHeight;
};
