#include "auto_tester.h"

#include <QtCore/QDateTime>
#include <QtCore/QTextStream>
#include <QtCore/QTimer>

#include <core/resource_management/resource_pool.h>
#include <utils/common/warnings.h>
#include <utils/common/command_line_parser.h>

namespace {
    qint64 defaultAutoTesterTimeout = 20 * 1000; 

    /** Run tests every second. */
    int defaultTestPeriod = 1000;
}

QnAutoTester::QnAutoTester(int &argc, char **argv, QObject *parent):
    QObject(parent),
    m_state(Initial),
    m_startTime(0),
    m_timeout(defaultAutoTesterTimeout),
    m_allTests(0),
    m_successfulTests(0),
    m_timer(NULL),
    m_succeeded(false)
{
    bool showHelp = false;

    QnCommandLineParser parser;
    parser.addParameter(&m_timeout,                 "--test-timeout",               NULL, tr("Time to wait before finishing the test, in milliseconds. Default is %1.").arg(defaultAutoTesterTimeout));
    parser.addParameter(&m_resourceSearchString,    "--test-resource-substring",    NULL, tr("Substring that must be present in one of the resources."));
    parser.addParameter(&showHelp,                  "--test-help",                  NULL, tr("Show this help screen."), true);

    bool valid = parser.parse(argc, argv, stderr);
    if(valid) {
        if(!m_resourceSearchString.isEmpty())
            m_allTests |= ResourceSubstring;
    } else {
        m_state = Invalid;
        showHelp = true;
    }

    if(showHelp) {
        QTextStream out(stdout);
        out << endl;
        out << "TESTS USAGE:" << endl;
        parser.print(out);
        out << endl;
    }
}
    
QnAutoTester::~QnAutoTester() {
    return;
}

void QnAutoTester::start() {
    if(m_state != Initial) {
        qnWarning("Cannot start auto tester that is not in its initial state.");
        return;
    }

    m_state = Running;

    m_timer = new QTimer(this);
    m_timer->setInterval(defaultTestPeriod);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(at_timer_timeout()));

    m_startTime = QDateTime::currentMSecsSinceEpoch();

    m_timer->start();
}

bool QnAutoTester::needsTesting(Test test) {
    return (m_allTests & test) && !(m_successfulTests & test);
}

void QnAutoTester::at_timer_timeout() {
    if(needsTesting(ResourceSubstring))
        testResourceSubstring();

    m_succeeded = m_successfulTests == m_allTests;

    if(m_succeeded || QDateTime::currentMSecsSinceEpoch() - m_startTime > m_timeout) {
        if(m_succeeded) {
            m_message = tr("All tests completed successfully.\n");
        } else {
            if(needsTesting(ResourceSubstring))
                m_message += tr("Test for resource substring '%1' failed.\n").arg(m_resourceSearchString);
        }

        m_state = Finished;
        m_timer->stop();
        emit finished();
    }
}

void QnAutoTester::testResourceSubstring() {
    foreach(const QnResourcePtr &resource, qnResPool->getResources()) {
        if(resource->getName().contains(m_resourceSearchString)) {
            m_successfulTests |= ResourceSubstring;
            return;
        }
    }
}

