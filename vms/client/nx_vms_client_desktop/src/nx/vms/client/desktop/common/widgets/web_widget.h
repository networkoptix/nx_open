#pragma once

#include <QtWidgets/QWidget>

class QUrl;

namespace nx::vms::client::desktop {

class WebEngineView;

/**
 * Adds busy indicator and simple error label on top of WebEngineView.
 */
class WebWidget: public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    WebWidget(QWidget* parent = nullptr);

    WebEngineView* webEngineView() const;

    void load(const QUrl& url);

private:
    WebEngineView* m_webEngineView = nullptr;
};

} // namespace nx::vms::client::desktop
