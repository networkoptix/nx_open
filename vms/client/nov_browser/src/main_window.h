#pragma once

#include <QtCore/QStringList>
#include <QtWidgets/QMainWindow>

#include <nx/core/layout/layout_file_info.h>
#include "layout_info.h"


namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private:
    void openFile();
    void checkFile();

    void clearData();
    void updateUI();

    void setPassword();
    void exportStream();

    Ui::MainWindow* ui;

    QString m_openDir;

    QString m_fileName;
    QString m_password;

    LayoutStructure m_structure;
    QStringList m_infoText;
};
