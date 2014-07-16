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

    virtual void loadData();
    virtual void submitData();

protected:
    void addPage(int key, QnAbstractPreferencesWidget *page, const QString &title);

    void setTabWidget(QTabWidget *tabWidget);
private:
    void initializeTabWidget();
private:
    QMap<int, QnAbstractPreferencesWidget*> m_pages;
    QPointer<QTabWidget> m_tabWidget;

};

#endif // GENERIC_TABBED_DIALOG_H
