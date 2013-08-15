#ifndef QN_MESSAGE_BOX_H
#define QN_MESSAGE_BOX_H

#include <QMessageBox>

class QnMessageBox: public QMessageBox {
    Q_OBJECT
    typedef QMessageBox base_type;

public:
    explicit QnMessageBox(QWidget *parent = NULL)
    QnMessageBox(Icon icon, const QString &title, const QString &text, StandardButtons buttons = NoButton, int helpTopicId = -1, QWidget *parent = NULL, Qt::WindowFlags flags = Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);

    static StandardButton information(QWidget *parent, const QString &title, const QString &text, StandardButtons buttons = Ok, StandardButton defaultButton = NoButton, int helpTopicId = -1);
    static StandardButton question(QWidget *parent, const QString &title, const QString &text, StandardButtons buttons = Ok, StandardButton defaultButton = NoButton, int helpTopicId = -1); 
    static StandardButton warning(QWidget *parent, const QString &title, const QString &text, StandardButtons buttons = Ok, StandardButton defaultButton = NoButton, int helpTopicId = -1);
    static StandardButton critical(QWidget *parent, const QString &title, const QString &text, StandardButtons buttons = Ok, StandardButton defaultButton = NoButton, int helpTopicId = -1);

        
};


#endif // QN_MESSAGE_BOX_H
