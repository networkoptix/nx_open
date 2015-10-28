#ifndef GENERIC_TABBED_DIALOG_H
#define GENERIC_TABBED_DIALOG_H

#include <ui/dialogs/button_box_dialog.h>

class QnAbstractPreferencesWidget;

class QnGenericTabbedDialog: public QnButtonBoxDialog {
    Q_OBJECT
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly)

    typedef QnButtonBoxDialog base_type;
public:
    explicit QnGenericTabbedDialog(QWidget *parent = 0);

    int currentPage() const;
    void setCurrentPage(int page);

    virtual void reject() override;
    virtual void accept() override;

    void forcedUpdate();
    bool tryClose(bool force);

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);
protected:
    void retranslateUi();

    void addPage(int key, QnAbstractPreferencesWidget *page, const QString &title);
    void setPageEnabled(int key, bool enabled);

    virtual void loadData();
    virtual void submitData();

    virtual bool confirm();
    virtual bool discard();

    virtual bool hasChanges() const;

    virtual QString confirmMessageTitle() const;
    virtual QString confirmMessageText() const;

    virtual void buttonBoxClicked(QDialogButtonBox::StandardButton button) override;

    virtual void initializeButtonBox() override;

    /** Update state of the dialog buttons. */
    virtual void updateButtonBox();
private:
    void initializeTabWidget();

    void setTabWidget(QTabWidget *tabWidget);
private:
    struct Page {
        int key;
        QString title;
        QnAbstractPreferencesWidget* widget;

        Page(): key(-1), widget(nullptr){}
    };

    QList<Page> modifiedPages() const;


    QList<Page> m_pages;
    QPointer<QTabWidget> m_tabWidget;
    bool m_readOnly;
    bool m_updating;

};

#endif // GENERIC_TABBED_DIALOG_H
