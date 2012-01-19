#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <QtGui/QDialog>

class QGroupBox;
class QLabel;

class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = 0);
    ~AboutDialog();

protected:
    void changeEvent(QEvent *event);

private:
    void retranslateUi();

private:
    Q_DISABLE_COPY(AboutDialog)

    QGroupBox *m_informationGroupBox;
    QLabel *m_versionLabel;

    QGroupBox *m_creditsGroupBox;
    QLabel *m_creditsLabel;
};

#endif // ABOUTDIALOG_H
