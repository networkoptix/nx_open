#pragma once

class AbstractAccessor;

/** Helper class to make several widgets always have the same width (yet). */
class QnAligner : public QObject
{
    Q_OBJECT
    typedef QObject base_type;

public:
    explicit QnAligner(QObject* parent = nullptr);
    virtual ~QnAligner();

    /** Add widget to align. Does not retain ownership. */
    void addWidget(QWidget* widget);

    /** Add several widgets to align. Does not retain ownership. */
    void addWidgets(std::initializer_list<QWidget*> widgets);

    /** Use custom accessor for the given widgets type. Takes ownership of an accessor. */
    void registerTypeAccessor(const QLatin1String& className, AbstractAccessor* accessor);

    /** Use custom accessor for the given widgets type. Takes ownership of an accessor. */
    template <class T>
    void registerTypeAccessor(AbstractAccessor* accessor)
    {
        registerTypeAccessor(QLatin1String(T::staticMetaObject.className()), accessor);
    }

    void start();

private:
    void align();

    AbstractAccessor* accessor(QWidget* widget) const;

private:
    QList<QWidget*> m_widgets;
    QHash<QLatin1String, AbstractAccessor *> m_accessorByClassName;
    QScopedPointer<AbstractAccessor> m_defaultAccessor;
};
