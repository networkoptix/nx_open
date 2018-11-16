#include "main_window.h"
#include "ui_main_window.h"

#include "layout_reader.h"

#include <nx/core/layout/layout_file_info.h>
#include <utils/crypt/crypted_file_stream.h>

#include <QtCore/QScopedPointer>
#include <QtCore/QStringList>
#include <QtCore/QStandardPaths>
#include <QtGui/QActionEvent>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QMessageBox>

using namespace nx::core;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->streamTable->setColumnWidth(0, 250);

    connect(ui->actionExit, &QAction::triggered, this, &QMainWindow::close);
    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::openFile);
    connect(ui->setPasswordButton, &QPushButton::pressed, this, &MainWindow::setPassword);
    connect(ui->exportStreamButton, &QPushButton::pressed, this, &MainWindow::exportStream);

    connect(ui->filenameLabel, &QPushButton::clicked, this, &MainWindow::openFile);
    connect(ui->streamTable, &QTableWidget::itemDoubleClicked, this, &MainWindow::exportStream);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::openFile()
{
    if (m_openDir.isEmpty())
        m_openDir = QStandardPaths::standardLocations(QStandardPaths::DesktopLocation).first();

    const auto fileName = QFileDialog::getOpenFileName(this, "Open Layout",
        m_openDir, "Layout Files (*.nov *.exe)");
    if (fileName.isEmpty())
        return;

    m_fileName = fileName;
    m_openDir = QFileInfo(m_fileName).absoluteDir().absolutePath();

    checkFile();

    readLayoutStructure(m_fileName, m_structure);
    if (!m_structure.info.isValid)
        m_infoText << m_structure.errorText;

    updateUI();

}

void MainWindow::checkFile()
{
    clearData();

    m_structure.info = layout::identifyFile(m_fileName);

    m_infoText.clear();
    m_infoText << ("File " + m_fileName + " is " + (m_structure.info.isValid ? "OK" : "NOT OK"));
    if (!m_structure.info.isValid)
        return;

    m_infoText << (QString("It is ") + (m_structure.info.isCrypted ? "ENCRYPTED" : "NOT ENCRYPTED"));
}

void MainWindow::clearData()
{
    m_structure = LayoutStructure();
    ui->logText->clear();
    m_password = QString();

    updateUI();
}

void MainWindow::updateUI()
{
    ui->logText->setPlainText(m_infoText.join("\n"));

    ui->filenameLabel->setText(m_fileName);
    ui->okCheckbox->setChecked(m_structure.info.isValid);
    ui->encryptedCheckbox->setChecked(m_structure.info.isCrypted);

    auto& table = ui->streamTable;
    table->clearContents();
    table->setRowCount(m_structure.streams.size());
    for(qint64 i = 0; i < m_structure.streams.size(); i++)
    {
        auto& stream = m_structure.streams[i];
        table->setItem(i, 0, new QTableWidgetItem(stream.name));
        table->setItem(i, 1, new QTableWidgetItem("0x" + QString::number(stream.position, 16)));
        table->setItem(i, 2, new QTableWidgetItem("0x" + QString::number(stream.size, 16)
            + " (" + QString::number(stream.size) + ")"));
        table->setItem(i, 3, new QTableWidgetItem("0x" + QString::number(stream.grossSize, 16)
            + " (" + QString::number(stream.grossSize) + ")"));
        table->setItem(i, 4, new QTableWidgetItem(stream.isCrypted ? "X" : ""));
    }

    ui->setPasswordButton->setEnabled(m_structure.info.isValid && m_structure.info.isCrypted);
}

void MainWindow::setPassword()
{
    const bool ok = checkPassword(ui->passwordEdit->text(), m_structure.info);
    ui->passwordOkLabel->setText(ok ? "OK" : "");
    m_infoText.append(QString("Password is ") + (ok ? "OK" : "NOT OK"));

    m_password = ok ? ui->passwordEdit->text() : QString();

    updateUI();
}

void MainWindow::exportStream()
{
    const int index = ui->streamTable->currentRow();
    if (index < 0)
    {
        QMessageBox::warning(this, "Error", "Select stream to export.");
        return;
    }

    if (m_password.isEmpty() && m_structure.streams[index].isCrypted)
    {
        QMessageBox::warning(this, "Error", "Set password to export encrypted stream.");
        return;
    }

    auto& streamInfo = m_structure.streams[index];

    const auto fileName = QFileDialog::getSaveFileName(this, "Save Stream",
        QFileInfo(m_openDir, streamInfo.name).absoluteFilePath(), "All Files (*.*)");

    if(!fileName.isEmpty())
    {
        QFile outFile(fileName);
        outFile.open(QIODevice::WriteOnly);
        if (!outFile.isOpen())
        {
            QMessageBox::warning(this, "Error", "Cannot write to file.");
            return;
        }

        QScopedPointer<QIODevice> readStream;
        if(streamInfo.isCrypted)
        {
            readStream.reset(new nx::utils::CryptedFileStream(m_fileName,
                streamInfo.position, streamInfo.grossSize, m_password));
            readStream->open(QIODevice::ReadOnly);
        }
        else
        {
            readStream.reset(new QFile(m_fileName));
            readStream->open(QIODevice::ReadOnly);
            readStream->seek(streamInfo.position);
        }

        if(!readStream->isOpen())
        {
            QMessageBox::warning(this, "Error", "The stream is damaged.");
            return;
        }

        // Actually do export.
        constexpr int bufferSize = 65536;
        char buffer[bufferSize];

        qint64 bytesToWrite = streamInfo.size;
        while (bytesToWrite > bufferSize)
        {
            readStream->read(buffer, bufferSize);
            outFile.write(buffer, bufferSize);
            bytesToWrite -= bufferSize;
        }
        readStream->read(buffer, bytesToWrite);
        outFile.write(buffer, bytesToWrite);
    }
    m_infoText.append(QString("Exported stream ") + streamInfo.name + " to " + fileName);
    updateUI();
}
