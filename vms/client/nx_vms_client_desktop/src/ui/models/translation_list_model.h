// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QString>
#include <QtGui/QIcon>

namespace nx::vms::client::desktop {

struct TranslationInfo
{
    /** Locale code, e.g. "zh_CN". */
    QString localeCode;

    /** Language name. */
    QString languageName;

    /** Country flag. */
    QIcon flag;
};

} // namespace nx::vms::client::desktop

class QnTranslationListModel: public QAbstractListModel
{
    Q_OBJECT
    using base_type = QAbstractListModel;
    using TranslationInfo = nx::vms::client::desktop::TranslationInfo;

public:
    enum Roles
    {
        /** Role for translations. Value of type TranslationInfo. */
        TranslationRole = Qt::UserRole,
    };

    QnTranslationListModel(QObject *parent = nullptr);
    virtual ~QnTranslationListModel();

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;

private:
    QList<TranslationInfo> m_translations;
};
