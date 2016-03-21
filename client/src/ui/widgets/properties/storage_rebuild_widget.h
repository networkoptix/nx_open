#pragma once

#include <QtWidgets/QWidget>

namespace Ui {
    class StorageRebuildWidget;
}

struct QnStorageScanData;

class QnStorageRebuildWidget: public QWidget {
    Q_OBJECT

    typedef QWidget base_type;  
public:
    QnStorageRebuildWidget(QWidget *parent = nullptr);
    virtual ~QnStorageRebuildWidget();

    void loadData(const QnStorageScanData &data);

signals:
    void cancelRequested();

private:
    QScopedPointer<Ui::StorageRebuildWidget> ui;
};
