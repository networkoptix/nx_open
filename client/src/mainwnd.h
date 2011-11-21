#ifndef MAINWND_H
#define MAINWND_H

#include <QtGui/QMainWindow>

class CLLayoutNavigator;
class LayoutContent;

class MainWnd : public QMainWindow
{
    Q_OBJECT

public:
    MainWnd(int argc, char* argv[], QWidget *parent = 0, Qt::WFlags flags = 0);
    ~MainWnd();

    static void findAcceptedFiles(QStringList& files, const QString& path);
    static MainWnd* instance() { return m_instance; }
    void addFilesToCurrentOrNewLayout(const QStringList& files, bool forceNewLayout = false);

    void goToNewLayoutContent(LayoutContent* newl);

private slots:
    void itemActivated(uint resourceId);
    void handleMessage(const QString& message);

private:
    void closeEvent ( QCloseEvent * event );
    void destroyNavigator(CLLayoutNavigator*& nav);

    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);

    void activate();

private:
    CLLayoutNavigator* m_normalView;
    static MainWnd* m_instance;
};

#endif // MAINWND_H
