#pragma once

#include <QtWidgets/QWidget>

namespace Ui {
    class StorageRebuildWidget;
} // namespace Ui

struct QnStorageScanData;

class QnStorageRebuildWidget : public QWidget
{
    Q_OBJECT
    using base_type = QWidget;

public:
    QnStorageRebuildWidget(QWidget* parent = nullptr);
    virtual ~QnStorageRebuildWidget();

    void loadData(const QnStorageScanData &data, bool isBackup);

signals:
    void cancelRequested();

private:
    QScopedPointer<Ui::StorageRebuildWidget> ui;
};
