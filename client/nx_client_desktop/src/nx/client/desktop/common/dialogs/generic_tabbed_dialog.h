#pragma once

#include <QtCore/QPointer>

#include <ui/dialogs/common/button_box_dialog.h>

class QTabWidget;

namespace nx {
namespace client {
namespace desktop {

/**
 *  Generic class to create multi-paged dialogs.
 *  Each page of the dialog must have its own unique key (enum is recommended). All pages work is done
 *  through these keys.
 *
 */
class GenericTabbedDialog: public QnButtonBoxDialog
{
    Q_OBJECT
    using base_type = QnButtonBoxDialog;

public:
    explicit GenericTabbedDialog(QWidget *parent = nullptr, Qt::WindowFlags windowFlags = 0);

    /**
     * @brief currentPage                       Get key of the current page.
     */
    int currentPage() const;

    /**
     * @brief setCurrentPage                    Select current page by key.
     * \param adjust                            If set to true, nearest available page will be
     *                                          selected if target page is disabled or hidden.
     */
    void setCurrentPage(int page, bool adjust = false);

protected:
    struct Page {
        int key;
        QString title;
        bool visible;
        bool enabled;
        QWidget* widget;

        Page(): key(-1), visible(true), enabled(true), widget(nullptr) {}
        bool isValid() const { return visible && enabled; }
    };

    /**
     * @brief addPage                           Add page, associated with the given key and the following title.
     */
    void addPage(int key, QWidget *page, const QString& title);

    /**
    * @brief isPageVisible                      Check if page is visible by its key.
    */
    bool isPageVisible(int key) const;

    /**
    * @brief setPageVisible                    Show or hide the page by its key.
    */
    void setPageVisible(int key, bool visible);

    /**
     * @brief setPageEnabled                    Enable or disable the page by its key.
     */
    void setPageEnabled(int key, bool enabled);

    virtual void initializeButtonBox() override;

    QList<Page> allPages() const;

protected:
    void setTabWidget(QTabWidget* tabWidget);

private:
    void initializeTabWidget();

private:
    QList<Page> m_pages;
    QPointer<QTabWidget> m_tabWidget;
};

} // namespace desktop
} // namespace client
} // namespace nx
