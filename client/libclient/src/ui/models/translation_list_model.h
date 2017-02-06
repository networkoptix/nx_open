#pragma once

#include <QtCore/QAbstractListModel>

#include <translation/translation.h>

class QnTranslationListModel: public QAbstractListModel
{
    Q_OBJECT
    typedef QAbstractListModel base_type;

public:
    QnTranslationListModel(QObject *parent = NULL);
    virtual ~QnTranslationListModel();

    const QList<QnTranslation> &translations() const;
    void setTranslations(const QList<QnTranslation> &translations);

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;

private:
    QList<QnTranslation> m_translations;
};
