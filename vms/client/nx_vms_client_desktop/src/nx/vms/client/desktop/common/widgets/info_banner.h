#pragma once

#include <QtWidgets/QLabel>

namespace nx::vms::client::desktop {

class InfoBanner: public QLabel
{
    using base_type = QLabel;

public:
    explicit InfoBanner(QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    explicit InfoBanner(
        const QString& text,
        QWidget* parent = nullptr,
        Qt::WindowFlags f = Qt::WindowFlags());

    void setWarningStyle(bool value);

private:
    void setupUi();
    void setWarningStyleInternal(bool value);

private:
    // Caching warning style to avoid multiple stylesheet recalculations.
    bool m_warningStyle = false;
};

} // namespace nx::vms::client::desktop
