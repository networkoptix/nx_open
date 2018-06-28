#include "file_name_input_widget.h"
#include "ui_file_name_input_widget.h"

QnFileNameInputWidget::QnFileNameInputWidget(
    const GetFileNameFunction& getFileName,
    QWidget* parent)
    :
    QWidget(parent),
    ui(new Ui::QnFileNameInputWidget)
{
    ui->setupUi(this);
    connect(ui->browseButton, &QPushButton::clicked, this,
        [this, getFileName]()
        {
            const auto fileName = getFileName();
            if (fileName.isEmpty())
                return;

            ui->fileNameEdit->setText(fileName);
            emit fileNameChanged(fileName);
        });
}

QnFileNameInputWidget::~QnFileNameInputWidget()
{
}
