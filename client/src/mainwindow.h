#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QtGui/QMainWindow>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = 0);

// ### removeme
public Q_SLOTS:
    void boo();
// ###
};

#endif // MAINWINDOW_H
