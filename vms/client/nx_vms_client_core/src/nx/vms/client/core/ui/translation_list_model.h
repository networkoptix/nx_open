// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QString>
#include <QtGui/QIcon>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API TranslationListModel: public QAbstractListModel
{
    Q_OBJECT
    using base_type = QAbstractListModel;

public:
    static void registerQmlType();

    enum Roles
    {
        LocaleCodeRole = Qt::UserRole,
    };

    TranslationListModel(QObject *parent = nullptr);
    virtual ~TranslationListModel();

    virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    virtual Qt::ItemFlags flags(const QModelIndex& index) const override;
    virtual QVariant data(const QModelIndex& index, int role) const override;

    virtual QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE int localeIndex(const QString& currentLocale);

private:
    struct TranslationInfo
    {
        /** Locale code, e.g. "zh_CN". */
        QString localeCode;

        /** Language name. */
        QString languageName;

        /** Country flag. */
        QIcon flag;
    };

    QList<TranslationInfo> m_translations;
};

} // namespace nx::vms::client::core
