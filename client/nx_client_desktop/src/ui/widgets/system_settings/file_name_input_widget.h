#pragma once

#include <QtWidgets/QWidget>

namespace Ui { class QnFileNameInputWidget; }

class QnFileNameInputWidget : public QWidget
{
    Q_OBJECT

public:
    using GetFileNameFunction = std::function<QString ()>;
    explicit QnFileNameInputWidget(
        const GetFileNameFunction& getFileName,
        QWidget* parent = nullptr);

    virtual ~QnFileNameInputWidget() override;

signals:
    void fileNameChanged(const QString& fileName);

private:
    QScopedPointer<Ui::QnFileNameInputWidget> ui;
};

