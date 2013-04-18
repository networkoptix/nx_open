#ifndef CUSTOM_FILE_DIALOG_H
#define CUSTOM_FILE_DIALOG_H

#include <QFileDialog>

class QnCustomFileDialog : public QFileDialog
{
    Q_OBJECT

    typedef QFileDialog base_type;

public:
    explicit QnCustomFileDialog(QWidget *parent = 0,
                         const QString &caption = QString(),
                         const QString &directory = QString(),
                         const QString &filter = QString());
    ~QnCustomFileDialog();

//    void addCheckbox(const QString &id, const QString &caption, const bool defaultValue);
    void addWidget(QWidget* widget);
};

#endif // CUSTOM_FILE_DIALOG_H
