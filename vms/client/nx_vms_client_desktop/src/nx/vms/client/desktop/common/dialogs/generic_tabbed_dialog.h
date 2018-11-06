#pragma once

#include <QtCore/QPointer>

#include <ui/dialogs/common/button_box_dialog.h>

class QTabWidget;

namespace nx::vms::client::desktop {

/**
 *  Generic class to create multi-paged dialogs. Each page of the dialog must have its own unique
 *  key (enum is recommended). Page changes interface is implemented through these keys.
 */
class GenericTabbedDialog: public QnButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnButtonBoxDialog;

public:
    explicit GenericTabbedDialog(QWidget* parent = nullptr, Qt::WindowFlags windowFlags = 0);

    static constexpr int kInvalidPage = -1;

    /**
     * Get key of the current page.
     */
    int currentPage() const;

    /**
     * Select current page by key.
     */
    void setCurrentPage(int key);

protected:
    /**
     * Add page, associated with the given key and the following title. Pages are displayed in the
     * order of adding.
     */
    void addPage(int key, QWidget* widget, const QString& title);

    /**
    * Check if page is visible by its key.
    */
    bool isPageVisible(int key) const;

    /**
    * Show or hide the page by its key.
    */
    void setPageVisible(int key, bool visible);

    /**
     * Enable or disable the page by its key.
     */
    void setPageEnabled(int key, bool enabled);

    virtual void initializeButtonBox() override;

    void setTabWidget(QTabWidget* tabWidget);

private:
    struct Page
    {
        int key = kInvalidPage;
        QString title;
        bool visible = true;
        bool enabled = true;
        QWidget* widget = nullptr;

        bool isValid() const { return visible && enabled; }
    };
    using Pages = QList<Page>;

    void initializeTabWidget();

    Pages::iterator findPage(int key);
    Pages::const_iterator findPage(int key) const;

private:
    QList<Page> m_pages;
    QPointer<QTabWidget> m_tabWidget;
};

} // namespace nx::vms::client::desktop
