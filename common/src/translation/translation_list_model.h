#ifndef QN_TRANSLATION_LIST_MODEL_H
#define QN_TRANSLATION_LIST_MODEL_H

#include <QtCore/QAbstractListModel>

#include "translation.h"

class QnTranslationListModel: public QAbstractListModel {
    Q_OBJECT
    typedef QAbstractListModel base_type;

public:
    QnTranslationListModel(QObject *parent = NULL);
    virtual ~QnTranslationListModel();

    const QList<QnTranslation> &translations() const;
    void setTranslations(const QList<QnTranslation> &translations);

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;
    virtual bool setData(const QModelIndex &index, const QVariant &value, int role) override;

private:
    static bool isInternal(const QnTranslation &translation);

private:
    QList<QnTranslation> m_translations;
    bool m_hasExternal;
};

#endif // QN_TRANSLATION_LIST_MODEL_H
