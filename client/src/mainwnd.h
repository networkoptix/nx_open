#ifndef MAINWND_H
#define MAINWND_H

#include <QScopedPointer>
#include <QtGui/QMainWindow>

class QnBlueBackgroundPainter;

class CLLayoutNavigator;
class LayoutContent;

class MainWnd : public QMainWindow
{
    Q_OBJECT

public:
    MainWnd(int argc, char* argv[], QWidget *parent = 0, Qt::WFlags flags = 0);
    virtual ~MainWnd();

    static MainWnd* instance() { return s_instance; }
    void addFilesToCurrentOrNewLayout(const QStringList& files, bool forceNewLayout = false);

    void goToNewLayoutContent(LayoutContent* newl);

private slots:
    void itemActivated(uint resourceId);
    void handleMessage(const QString& message);

private:
    void closeEvent(QCloseEvent *event);
    void destroyNavigator(CLLayoutNavigator*& nav);

#if 0
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);
#endif

    void activate();

private:
    QScopedPointer<QnBlueBackgroundPainter> m_backgroundPainter;
    CLLayoutNavigator* m_normalView;
    static MainWnd *s_instance;
};

#endif // MAINWND_H
