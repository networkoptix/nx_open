#pragma once

class QnAbstractResourceSearcher;

class QnMediaServerResourceSearchers: public QObject
{
    Q_OBJECT
public:
    QnMediaServerResourceSearchers(QObject* parent = nullptr);
    virtual ~QnMediaServerResourceSearchers();

private:
    QList<QnAbstractResourceSearcher*> m_searchers;
};



