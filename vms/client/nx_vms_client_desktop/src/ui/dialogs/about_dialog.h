// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_ABOUT_DIALOG_H
#define QN_ABOUT_DIALOG_H

#include <QtCore/QScopedPointer>

#include <core/resource/resource_fwd.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/workbench/workbench_context_aware.h>

namespace Ui {
    class AboutDialog;
}

class QnResourceListModel;

class QnAboutDialog : public QnSessionAwareButtonBoxDialog {
    Q_OBJECT

    typedef QnSessionAwareButtonBoxDialog base_type;
public:
    explicit QnAboutDialog(QWidget *parent = 0);
    virtual ~QnAboutDialog();

protected:
    virtual void changeEvent(QEvent *event) override;

protected slots:
    void at_copyButton_clicked();

private:
    void initSaasSupportInfo();
    void retranslateUi();

    // Makes report that contains a list of connected servers and their versions.
    void generateServersReport();

private:
    QScopedPointer<Ui::AboutDialog> ui;
    QPushButton *m_copyButton;

    QString m_serversReport;
    QnResourceListModel* m_serverListModel = nullptr;
};

#endif // QN_ABOUT_DIALOG_H
