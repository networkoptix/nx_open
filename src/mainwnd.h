#ifndef MAINWND_H
#define MAINWND_H

#include "ui_mainwnd.h"
#include "ui/ui_common.h"

class CLLayoutNavigator;
class LayoutContent;

class MainWnd : public QWidget
{
    Q_OBJECT

public:
    MainWnd(int argc, char* argv[], QWidget *parent = 0, Qt::WFlags flags = 0);
    ~MainWnd();

    static void findAcceptedFiles(QStringList& files, const QString& path);
    static MainWnd* instance() { return m_instance; }
    void addFilesToCurrentOrNewLayout(const QStringList& files, bool forceNewLayout = false);
private slots:
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
