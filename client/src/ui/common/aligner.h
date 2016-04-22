#pragma once

/** Helper class to make several widgets always have the same width (yet). */
class QnAligner : public QObject
{
    Q_OBJECT
    typedef QObject base_type;

public:
    QnAligner(QObject* parent = nullptr);
    virtual ~QnAligner();

    void addWidget(QWidget* widget);

    void start();

private:
    void align();

private:
    QList<QWidget*> m_widgets;
};
