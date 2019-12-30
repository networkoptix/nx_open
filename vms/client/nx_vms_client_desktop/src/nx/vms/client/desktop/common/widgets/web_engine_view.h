#pragma once

#include <chrono>

#include <QtWebEngineWidgets/QWebEngineView>
#include <QtWebEngineWidgets/QWebEngineProfile>

namespace nx::vms::client::desktop {

/**
 * A replacement for standard QWebEngineView.
 * Allows to customize various parameters in one place
 * (user agent, context menus, link delegation, styles, ignore ssl errors etc.)
 */
class WebEngineView: public QWebEngineView
{
    Q_OBJECT
    using base_type = QWebEngineView;

public:
    WebEngineView(QWidget* parent = nullptr, WebEngineView* deriveFrom = nullptr);
    virtual ~WebEngineView();

    // Replicates setContextMenuPolicy(Qt::ActionsContextMenu) behavior for link context menu.
    void setUseActionsForLinks(bool enabled);
    bool useActionsForLinks() const;

    // Enables ignoring SSL certificate errors.
    void setIgnoreSslErrors(bool ignore);
    bool hasIgnoreSslErrors() const;

    // Creates web page with new off-the-records profile.
    void createPageWithNewProfile();

    // Enables redirecting all link navigation to desktop browser.
    void setRedirectLinksToDesktop(bool enabled);
    bool isRedirectLinksToDesktop() const;

    // Synchronously wait for page to load.
    void waitLoadFinished(std::chrono::milliseconds timeout);

    void insertStyleSheet(const QString& name, const QString& source, bool immediately = true);
    void removeStyleSheet(const QString& name, bool immediately = true);

    void setHiddenActions(const std::vector<QWebEnginePage::WebAction> actions);

signals:
    void profileChanged();

protected:
    virtual void contextMenuEvent(QContextMenuEvent* event) override;
    QWebEngineView* createWindow(QWebEnginePage::WebWindowType type) override;

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // namespace nx::vms::client::desktop