#pragma once

#include <ui/dialogs/button_box_dialog.h>

class QnAbstractPreferencesWidget;

/** 
 *  Generic class to create multi-paged dialogs. Designed to use with default OK-Apply-Cancel scenario.
 *  Each page of the dialog must have its own unique key (enum is recommended). All pages work is done
 *  through these keys. 
 * 
 *  Derived dialogs basically should implement the following behavior model:
 *  *   create pages in constructor
 *  *   call loadDataToUi method every time the source model changes //TODO: #GDM make more strict way
 *  *   every page must implement loadDataToUi / hasChanges / applyChanges methods
 *  *   every page should emit hasChangesChanged in all cases where new changes can appear
 */
class QnGenericTabbedDialog: public QnButtonBoxDialog {
    Q_OBJECT
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)

    typedef QnButtonBoxDialog base_type;
public:
    explicit QnGenericTabbedDialog(QWidget *parent = nullptr, Qt::WindowFlags windowFlags = 0);

    /** 
     * @brief currentPage                       Get key of the current page.
     */
    int currentPage() const;

    /** 
     * @brief setCurrentPage                    Select current page by key. 
     */
    void setCurrentPage(int page);

    /** 
      * @brief reject                           Overriding default reject method. Here the dialog will try to revert 
      *                                         all changes (if any were applied instantly). If any page cannot revert
      *                                         its changes, dialog will not be closed. Otherwise, dialog will close 
      *                                         and all its contents will be reloaded. //TODO: #GDM why not on display?
      */ 
    virtual void reject() override;

    /** 
     * @brief accept                            Overriding default accept method. Here the dialog
     *                                          will try to apply all changes (if possible). 
     *                                          If any page cannot apply its changes, dialog will not be closed.
     */ 
    virtual void accept() override;

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);
protected:
    struct Page {
        int key;
        QString title;
        QnAbstractPreferencesWidget* widget;

        Page(): key(-1), widget(nullptr){}
    };

    /** 
     * @brief addPage                           Add page, associated with the given key and the following title. 
     */
    void addPage(int key, QnAbstractPreferencesWidget *page, const QString &title);

    /** 
     * @brief setPageEnabled                    Enable or disable the page by its key.
     */
    void setPageEnabled(int key, bool enabled);

     /** 
      * @brief retranslateUi                    Update dialog ui if source data was changed. 
      *                                         Usually is overridden to update dialog title. 
      *                                         Must call parent method in the implementation.
      */
    virtual void retranslateUi();

    /** 
      * @brief loadDataToUi                     Refresh ui state based on model data. 
      *                                         Should be called from derived classes. 
      */
    virtual void loadDataToUi();

    /** 
      * @brief applyChanges                     Apply changes to model. Called automatically.
      */
    virtual void applyChanges();

    /**
     * @brief canApplyChanges                   Check that all values are correct so saving is possible.
     * @return                                  False if saving must be aborted, true otherwise.
     */
    virtual bool canApplyChanges();

    /**
     * @brief canDiscardChanges                 Check that all changes can be discarded safely.
     *                                          Usually is called before dialog is closed by "Cancel" button.
     * @return                                  False if closing must be aborted, true otherwise.
     */
    virtual bool canDiscardChanges();

    /**
     * @brief hasChanges                        Check if there are any unsaved changes in the dialog.
     * @return                                  True if there are any changes, false otherwise.
     */
    virtual bool hasChanges() const;



    virtual void buttonBoxClicked(QDialogButtonBox::StandardButton button) override;

    virtual void initializeButtonBox() override;

    virtual bool forcefullyClose();

    /** Update state of the dialog buttons. */
    virtual void updateButtonBox();

    QList<Page> allPages() const;
    QList<Page> modifiedPages() const;
private:
    void initializeTabWidget();

    void setTabWidget(QTabWidget *tabWidget);
private:
    QList<Page> m_pages;
    QPointer<QTabWidget> m_tabWidget;
    bool m_readOnly;
    bool m_updating;

};
