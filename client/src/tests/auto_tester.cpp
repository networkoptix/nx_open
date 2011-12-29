#include "auto_tester.h"
#include <QDateTime>
#include <QTextStream>
#include <QTimer>
#include <core/resourcemanagment/resource_pool.h>
#include "command_line_parser.h"
#include "auto_test_result.h"

namespace {
    qint64 defaultAutoTesterTimeout = 20 * 1000; 
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
    QnCommandLineParser parser;
    parser.addParameter(QnCommandLineParameter(QnCommandLineParameter::INTEGER, "--test-timeout", NULL, tr("Time to wait before finishing the test, in milliseconds.")));
    parser.addParameter(QnCommandLineParameter(QnCommandLineParameter::STRING, "--test-resource-substring", NULL, tr("Substring the must be present in one of the resources.")));

    m_valid = parser.parse(argc, argv);
    if(m_valid) {
        m_timeout = parser.value("--test-timeout", m_timeout).toLongLong();
        
        m_resourceSearchString = parser.value("--test-resource-substring", m_resourceSearchString).toString();
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
    if(QDateTime::currentMSecsSinceEpoch() - m_startTime > m_timeout) {
        bool success = m_successfulTests == m_allTests;

        QString message;
        if(success) {
            message = tr("All tests completed successfully.\n");
        } else {
            if(needsTesting(RESOURCE_SUBSTRING))
                message += tr("Test for resource substring '%1' failed.\n").arg(m_resourceSearchString);
        }

        throw QnAutoTestResult(success, message);
    }

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

