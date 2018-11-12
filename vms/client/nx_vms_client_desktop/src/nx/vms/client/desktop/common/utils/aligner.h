#pragma once

#include <initializer_list>

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QPointer>
#include <QtCore/QScopedPointer>

class QWidget;

namespace nx::vms::client::desktop {

class AbstractAccessor;

/** Helper class to make several widgets always have the same width (yet). */
class Aligner: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    explicit Aligner(QObject* parent = nullptr);
    virtual ~Aligner();

    /** Add widget to align. Does not retain ownership. */
    void addWidget(QWidget* widget);

    /** Add several widgets to align. Does not retain ownership. */
    void addWidgets(std::initializer_list<QWidget*> widgets);

    /** Add linked aligner to align with. Does not retain ownership. */
    void addAligner(Aligner* aligner);

    /** Use custom accessor for the given widgets type. Takes ownership of an accessor. */
    void registerTypeAccessor(const QLatin1String& className, AbstractAccessor* accessor);

    /** Use custom accessor for the given widgets type. Takes ownership of an accessor. */
    template <class T>
    void registerTypeAccessor(AbstractAccessor* accessor)
    {
        registerTypeAccessor(QLatin1String(T::staticMetaObject.className()), accessor);
    }

    /** Clear current set of widgets to align. */
    void clear();

    void align();

    bool skipInvisible() const;
    void setSkipInvisible(bool value);

    int minimumWidth() const;
    void setMinimumWidth(int value);

    /** Maximum width of all widgets. */
    int maxWidth() const;

    /** Set width of all widgets. */
    void enforceWidth(int width);

private:
    AbstractAccessor* accessor(QWidget* widget) const;

private:
    QList<QWidget*> m_widgets;
    QList<Aligner*> m_aligners;
    QHash<QLatin1String, AbstractAccessor *> m_accessorByClassName;
    QScopedPointer<AbstractAccessor> m_defaultAccessor;
    QPointer<Aligner> m_masterAligner;
    bool m_skipInvisible = false;
    int m_minimumWidth = 0;
};

} // namespace nx::vms::client::desktop
