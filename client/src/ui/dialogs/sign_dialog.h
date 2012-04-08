#ifndef SIGNDIALOG_H
#define SIGNDIALOG_H

#include <QtGui/QDialog>
#include "plugins/resources/archive/avi_files/avi_resource.h"

class QDataWidgetMapper;
class QStandardItemModel;
class QUrl;

class QnWorkbenchContext;
class QnAbstractArchiveReader;
class QnResourceWidgetRenderer;
class QnSignDialogGlWidget;
class CLCamDisplay;

namespace Ui {
    class SignDialog;
}

class SignDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SignDialog(const QString& fileName, QWidget *parent = 0);
    virtual ~SignDialog();

    static QRect calcVideoRect(double windowWidth, double windowHeight, double textureWidth, double textureHeight);
public slots:
    virtual void accept() override;
private slots:
    void at_calcSignInProgress(QByteArray sign, int progress);
    void at_gotImageSize(int width, int height);
protected:
    virtual void changeEvent(QEvent *event) override;

private:
    Q_DISABLE_COPY(SignDialog)

    QScopedPointer<Ui::SignDialog> ui;
    QWeakPointer<QnWorkbenchContext> m_context;
    QStandardItemModel *m_connectionsModel;
    int m_requestHandle;

    QnAviResourcePtr aviRes;
    QnSignDialogGlWidget* glWindow;
    QnAbstractArchiveReader* m_reader;
    QnResourceWidgetRenderer* renderer;
    CLCamDisplay* m_camDispay;
    QString m_fileName;
};

#endif // SIGNDIALOG_H
