#ifndef MAINWND_H
#define MAINWND_H

#include <QScopedPointer>
#include <QtGui/QMainWindow>

class CLLayoutNavigator;
class LayoutContent;

class QnBlueBackgroundPainter;
class QnWorkbenchController;

class MainWnd : public QMainWindow
{
    Q_OBJECT

public:
    MainWnd(int argc, char* argv[], QWidget *parent = 0, Qt::WFlags flags = 0);
    virtual ~MainWnd();

    static MainWnd* instance() { return s_instance; }
    void addFilesToCurrentOrNewLayout(const QStringList& files, bool forceNewLayout = false);

    void goToNewLayoutContent(LayoutContent* newl);

Q_SIGNALS:
    void mainWindowClosed();

protected:
    void closeEvent(QCloseEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);

private Q_SLOTS:
    void itemActivated(uint resourceId);
    void handleMessage(const QString &message);

private:
    void closeEvent(QCloseEvent *event);
    void destroyNavigator(CLLayoutNavigator *&nav);

    void activate();

private:
    QScopedPointer<QnBlueBackgroundPainter> m_backgroundPainter;
    QnWorkbenchController *m_controller;
    CLLayoutNavigator *m_normalView;
    static MainWnd *s_instance;
};

#endif // MAINWND_H
