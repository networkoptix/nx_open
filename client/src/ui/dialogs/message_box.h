#ifndef QN_MESSAGE_BOX_H
#define QN_MESSAGE_BOX_H

#include <QMessageBox>

class QnMessageBox: public QMessageBox {
    Q_OBJECT
    typedef QMessageBox base_type;

public:
    explicit QnMessageBox(QWidget *parent = NULL);
    QnMessageBox(Icon icon, int helpTopicId, const QString &title, const QString &text, StandardButtons buttons = NoButton, QWidget *parent = NULL, Qt::WindowFlags flags = Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);

    using base_type::information;
    using base_type::question;
    using base_type::warning;
    using base_type::critical;

    static StandardButton information(QWidget *parent, int helpTopicId, const QString &title, const QString &text, StandardButtons buttons = Ok, StandardButton defaultButton = NoButton);
    static StandardButton question(QWidget *parent, int helpTopicId, const QString &title, const QString &text, StandardButtons buttons = Ok, StandardButton defaultButton = NoButton); 
    static StandardButton warning(QWidget *parent, int helpTopicId, const QString &title, const QString &text, StandardButtons buttons = Ok, StandardButton defaultButton = NoButton);
    static StandardButton critical(QWidget *parent, int helpTopicId, const QString &title, const QString &text, StandardButtons buttons = Ok, StandardButton defaultButton = NoButton);
        
};


#endif // QN_MESSAGE_BOX_H
