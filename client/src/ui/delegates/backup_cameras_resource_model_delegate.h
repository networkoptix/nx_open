#pragma once

#include <client/client_color_types.h>

#include <ui/delegates/resource_pool_model_custom_column_delegate.h>

class QnBackupCamerasResourceModelDelegate: public QnResourcePoolModelCustomColumnDelegate {
    Q_OBJECT

    typedef QnResourcePoolModelCustomColumnDelegate base_type;
public:
    QnBackupCamerasResourceModelDelegate(QObject* parent = nullptr);
    virtual ~QnBackupCamerasResourceModelDelegate();

    virtual Qt::ItemFlags flags(const QModelIndex &index) const override;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role) override;

    const QnBackupCamerasColors &colors() const;
    void setColors(const QnBackupCamerasColors &colors);
private:
    QnBackupCamerasColors m_colors;
};
