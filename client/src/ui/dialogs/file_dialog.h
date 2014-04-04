#ifndef FILE_DIALOG_H
#define FILE_DIALOG_H

#include <QtWidgets/QFileDialog>

class QnFileDialog : public QFileDialog {
public:
    static QString getExistingDirectory(QWidget *parent = 0,
                                        const QString &caption = QString(),
                                        const QString &dir = QString(),
                                        QFileDialog::Options options = QFileDialog::ShowDirsOnly);

    static QString getOpenFileName(QWidget *parent = 0,
                                   const QString &caption = QString(),
                                   const QString &dir = QString(),
                                   const QString &filter = QString(),
                                   QString *selectedFilter = 0,
                                   QFileDialog::Options options = 0);

    static QStringList getOpenFileNames(QWidget *parent = 0,
                                        const QString &caption = QString(),
                                        const QString &dir = QString(),
                                        const QString &filter = QString(),
                                        QString *selectedFilter = 0,
                                        QFileDialog::Options options = 0);

    static QString getSaveFileName(QWidget *parent = 0,
                                   const QString &caption = QString(),
                                   const QString &dir = QString(),
                                   const QString &filter = QString(),
                                   QString *selectedFilter = 0,
                                   QFileDialog::Options options = 0);
};

#endif // FILE_DIALOG_H
