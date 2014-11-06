#ifndef GENERIC_TABBED_DIALOG_H
#define GENERIC_TABBED_DIALOG_H

#include <ui/dialogs/button_box_dialog.h>

class QnAbstractPreferencesWidget;

class QnGenericTabbedDialog: public QnButtonBoxDialog {
    Q_OBJECT

    typedef QnButtonBoxDialog base_type;
public:
    explicit QnGenericTabbedDialog(QWidget *parent = 0);

    int currentPage() const;
    void setCurrentPage(int page);

    virtual void reject() override;
    virtual void accept() override;

    void forcedUpdate();
    bool tryClose(bool force);
protected:
    void addPage(int key, QnAbstractPreferencesWidget *page, const QString &title);

    virtual void loadData();
    virtual void submitData();

    virtual bool confirm();
    virtual bool discard();

    virtual bool hasChanges() const;

private:
    void initializeTabWidget();

    void setTabWidget(QTabWidget *tabWidget);
private:
    struct Page {
        int key;
        QString title;
        QnAbstractPreferencesWidget* widget;
    };

    QList<Page> modifiedPages() const;


    QList<Page> m_pages;
    QPointer<QTabWidget> m_tabWidget;

};

#endif // GENERIC_TABBED_DIALOG_H
