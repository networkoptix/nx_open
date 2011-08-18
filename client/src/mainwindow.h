#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtGui/QMainWindow>

class GraphicsView;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = 0);

// ### removeme
public Q_SLOTS:
    void boo();
// ###

private:
    GraphicsView *m_view;
};

#endif // MAINWINDOW_H
