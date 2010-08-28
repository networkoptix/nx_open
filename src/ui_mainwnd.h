/********************************************************************************
** Form generated from reading UI file 'mainwnd.ui'
**
** Created: Fri Aug 27 20:04:17 2010
**      by: Qt User Interface Compiler version 4.6.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWND_H
#define UI_MAINWND_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtGui/QApplication>
#include <QtGui/QButtonGroup>
#include <QtGui/QHeaderView>
#include <QtGui/QMainWindow>
#include <QtGui/QMenuBar>
#include <QtGui/QStatusBar>
#include <QtGui/QToolBar>
#include <QtGui/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWndClass
{
public:
    QMenuBar *menuBar;
    QToolBar *mainToolBar;
    QWidget *centralWidget;
    QStatusBar *statusBar;

    void setupUi(QMainWindow *MainWndClass)
    {
        if (MainWndClass->objectName().isEmpty())
            MainWndClass->setObjectName(QString::fromUtf8("MainWndClass"));
        MainWndClass->resize(600, 400);
        menuBar = new QMenuBar(MainWndClass);
        menuBar->setObjectName(QString::fromUtf8("menuBar"));
        MainWndClass->setMenuBar(menuBar);
        mainToolBar = new QToolBar(MainWndClass);
        mainToolBar->setObjectName(QString::fromUtf8("mainToolBar"));
        MainWndClass->addToolBar(mainToolBar);
        centralWidget = new QWidget(MainWndClass);
        centralWidget->setObjectName(QString::fromUtf8("centralWidget"));
        MainWndClass->setCentralWidget(centralWidget);
        statusBar = new QStatusBar(MainWndClass);
        statusBar->setObjectName(QString::fromUtf8("statusBar"));
        MainWndClass->setStatusBar(statusBar);

        retranslateUi(MainWndClass);

        QMetaObject::connectSlotsByName(MainWndClass);
    } // setupUi

    void retranslateUi(QMainWindow *MainWndClass)
    {
        MainWndClass->setWindowTitle(QApplication::translate("MainWndClass", "MainWnd", 0, QApplication::UnicodeUTF8));
    } // retranslateUi

};

namespace Ui {
    class MainWndClass: public Ui_MainWndClass {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWND_H
