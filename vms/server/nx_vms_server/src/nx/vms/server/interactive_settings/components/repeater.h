#pragma once

#include "group.h"

namespace nx::vms::server::interactive_settings::components {

class Repeater: public Group
{
    Q_OBJECT
    Q_PROPERTY(QVariant template READ itemTemplate WRITE setItemTemplate
            NOTIFY itemTemplateChanged)
    Q_PROPERTY(int count READ count WRITE setCount NOTIFY countChanged)
    Q_PROPERTY(int startIndex READ startIndex WRITE setStartIndex NOTIFY startIndexChanged)
    Q_PROPERTY(QString addButtonCaption READ addButtonCaption WRITE setAddButtonCaption
            NOTIFY addButtonCaptionChanged)

    using base_type = Group;

public:
    Repeater(QObject* parent = nullptr);

    QVariant itemTemplate() const;
    void setItemTemplate(const QVariant& itemTemplate);

    int count() const { return m_count; }
    void setCount(int count);

    int startIndex() const { return m_startIndex; }
    void setStartIndex(int index);

    QString addButtonCaption() const { return m_addButtonCaption; }
    void setAddButtonCaption(const QString& caption);

    virtual QJsonObject serialize() const override;

signals:
    void itemTemplateChanged();
    void countChanged();
    void startIndexChanged();
    void addButtonCaptionChanged();

private:
    QJsonObject m_itemTemplate;
    int m_count = 0;
    int m_startIndex = 1;
    QString m_addButtonCaption;
};

} // namespace nx::vms::server::interactive_settings::components
