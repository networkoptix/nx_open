#include "auto_tester.h"
#include <QDateTime>
#include <QTextStream>
#include <QHash>
#include <QVariant>
#include <QPair>
#include <QTimer>
#include <QApplication> /* For Q_DECLARE_TR_FUNCTIONS. */
#include <utils/common/warnings.h>
#include <core/resourcemanagment/resource_pool.h>

namespace {
    qint64 defaultAutoTesterTimeout = 20 * 1000; 

    class QnCommandLineParameter {
    public:
        enum Type {
            STRING,
            INTEGER,
            FLAG,
        };

        QnCommandLineParameter(Type type, const QString &name, const QString &shortName, const QString &description):
            m_type(type), m_name(name), m_shortName(shortName), m_description(description)
        {}

        Type type() const {
            return m_type;
        }

        const QString &name() const {
            return m_name;
        }

        const QString &shortName() const {
            return m_shortName;
        }

        const QString &description() const {
            return m_description;
        }

        bool hasValue() const {
            return m_type != FLAG;
        }

    private:
        Type m_type;
        QString m_name;
        QString m_shortName;
        QString m_description;
    };

    class QnAutoTesterCommandLineParser {
        Q_DECLARE_TR_FUNCTIONS(QnAutoTesterCommandLineParser);
    public:
        QnAutoTesterCommandLineParser() {}

        void addParameter(const QnCommandLineParameter &parameter) {
            int index = m_parameters.size();
            m_parameters.push_back(parameter);
            m_values.push_back(QVariant());

            if(!parameter.name().isEmpty())
                m_indexByName[parameter.name()] = index;
            if(!parameter.shortName().isEmpty())
                m_indexByName[parameter.shortName()] = index;
        }

        QVariant value(const QString &name, const QVariant &defaultValue = QVariant()) {
            int index = m_indexByName.value(name, -1);
            if(index == -1)
                return defaultValue;

            return m_values[index];
        }

        void clear() {
            m_parameters.clear();
            m_values.clear();
            m_indexByName.clear();
        }

        bool parse(int &argc, char **argv) {
            int pos = 0, skipped = 0;
            
            QTextStream err(stderr);

            while(pos < argc) {
                /* Extract name. */
                QString name = QLatin1String(argv[pos]);
                int index = m_indexByName.value(name, -1);
                if(index == -1) {
                    argv[skipped] = argv[pos];
                    
                    pos++;
                    skipped++;
                    continue;
                }

                const QnCommandLineParameter &parameter = m_parameters[index];
                
                /* Extract string value if needed. */
                QString stringValue;
                if(parameter.hasValue()) {
                    pos++;
                    if(pos > argc) {
                        err << tr("No value provided for the '%1' parameter.").arg(name);
                        return false;
                    } else {
                        stringValue = QLatin1String(argv[pos]);
                    }
                }

                /* Convert to typed value. */
                QVariant value;
                switch(parameter.type()) {
                case QnCommandLineParameter::STRING:
                    value = stringValue;
                    break;
                case QnCommandLineParameter::INTEGER: {
                    bool ok;
                    qint64 intValue = stringValue.toLongLong(&ok);
                    if(!ok) {
                        err << tr("Invalid value for '%1' parameter - expected integer, provided '%2'.").arg(name).arg(stringValue);
                        return false;
                    } else {
                        value = intValue;
                    }
                    break;
                }
                case QnCommandLineParameter::FLAG:
                    value = true;
                    break;
                default:
                    qnWarning("Invalid command line parameter type '%1'.", static_cast<int>(parameter.type()));
                    break;
                }

                /* Store value. */
                m_values[index] = value;

                pos++;
            }

            argc = skipped;
            return true;
        }

    private:
        QList<QnCommandLineParameter> m_parameters;
        QList<QVariant> m_values;
        QHash<QString, int> m_indexByName;
    };

}

QnAutoTester::QnAutoTester(int &argc, char **argv, QObject *parent):
    QObject(parent),
    m_startTime(0),
    m_timeout(defaultAutoTesterTimeout),
    m_valid(true),
    m_started(false),
    m_successfulTests(0),
    m_allTests(0)
{
    QnAutoTesterCommandLineParser parser;
    parser.addParameter(QnCommandLineParameter(QnCommandLineParameter::INTEGER, QLatin1String("--test-timeout"), QString(), tr("Time to wait before finishing the test, in milliseconds.")));
    parser.addParameter(QnCommandLineParameter(QnCommandLineParameter::STRING, QLatin1String("--test-resource-substring"), QString(), tr("Substring the must be present in one of the resources.")));

    m_valid = parser.parse(argc, argv);
    if(m_valid) {
        m_timeout = parser.value(QLatin1String("--test-timeout"), m_timeout).toLongLong();
        
        m_resourceSearchString = parser.value(QLatin1String("--test-resource-substring"), m_resourceSearchString).toString();
        if(!m_resourceSearchString.isEmpty())
            m_allTests |= RESOURCE_SUBSTRING;
    }

    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(at_timer_timeout()));
}
    
QnAutoTester::~QnAutoTester() {
    return;
}

void QnAutoTester::start() {
    if(!m_valid)
        return;

    if(m_started)
        return;

    m_started = true;

    m_startTime = QDateTime::currentMSecsSinceEpoch();

    if(m_allTests != 0)
        m_timer->start();
}

bool QnAutoTester::needsTesting(Test test) {
    return (m_allTests & test) && !(m_successfulTests & test);
}

void QnAutoTester::at_timer_timeout() {
    if(QDateTime::currentMSecsSinceEpoch() - m_startTime > m_timeout)
        //throw std::exception("Tests failed.");

    if(needsTesting(RESOURCE_SUBSTRING))
        testResourceSubstring();
}

void QnAutoTester::testResourceSubstring() {
    foreach(const QnResourcePtr &resource, qnResPool->getResources()) {
        if(resource->toString().contains(m_resourceSearchString)) {
            m_successfulTests |= RESOURCE_SUBSTRING;
            return;
        }
    }
}

