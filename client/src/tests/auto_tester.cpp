#include "auto_tester.h"
#include <QDateTime>
#include <QTextStream>
#include <QTimer>
#include <core/resourcemanagment/resource_pool.h>
#include <utils/common/warnings.h>
#include <utils/common/command_line_parser.h>
#include "utils/common/synctime.h"

namespace {
    qint64 defaultAutoTesterTimeout = 20 * 1000; 

    /** Run tests every second. */
    int defaultTestPeriod = 1000;
}

QnAutoTester::QnAutoTester(int &argc, char **argv, QObject *parent):
    QObject(parent),
    m_state(INITIAL),
    m_startTime(0),
    m_timeout(defaultAutoTesterTimeout),
    m_successfulTests(0),
    m_allTests(0),
    m_timer(NULL),
    m_succeeded(false)
{
    QnCommandLineParser parser;
    parser.addParameter(QMetaType::Int,     "--test-timeout",               NULL, tr("Time to wait before finishing the test, in milliseconds. Default is %1.").arg(defaultAutoTesterTimeout));
    parser.addParameter(QMetaType::QString, "--test-resource-substring",    NULL, tr("Substring that must be present in one of the resources."));
    parser.addParameter(QMetaType::Bool,    "--test-help",                  NULL, tr("Show this help screen."), true);

    bool valid = parser.parse(argc, argv, stderr);
    bool showHelp = false;
    if(valid) {
        m_timeout = parser.value("--test-timeout", m_timeout).toLongLong();

        showHelp = parser.value("--test-help", false).toBool();

        m_resourceSearchString = parser.value("--test-resource-substring", m_resourceSearchString).toString();
        if(!m_resourceSearchString.isEmpty())
            m_allTests |= RESOURCE_SUBSTRING;
    } else {
        m_state = INVALID;
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
    if(m_state != INITIAL) {
        qnWarning("Cannot start auto tester that is not in its initial state.");
        return;
    }

    m_state = RUNNING;

    m_timer = new QTimer(this);
    m_timer->setInterval(defaultTestPeriod);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(at_timer_timeout()));

    m_startTime = qnSyncTime->currentMSecsSinceEpoch();

    m_timer->start();
}

bool QnAutoTester::needsTesting(Test test) {
    return (m_allTests & test) && !(m_successfulTests & test);
}

void QnAutoTester::at_timer_timeout() {
    if(needsTesting(RESOURCE_SUBSTRING))
        testResourceSubstring();

    m_succeeded = m_successfulTests == m_allTests;

    if(m_succeeded || qnSyncTime->currentMSecsSinceEpoch() - m_startTime > m_timeout) {
        if(m_succeeded) {
            m_message = tr("All tests completed successfully.\n");
        } else {
            if(needsTesting(RESOURCE_SUBSTRING))
                m_message += tr("Test for resource substring '%1' failed.\n").arg(m_resourceSearchString);
        }

        m_state = FINISHED;
        m_timer->stop();
        emit finished();
    }
}

void QnAutoTester::testResourceSubstring() {
    foreach(const QnResourcePtr &resource, qnResPool->getResources()) {
        if(resource->getName().contains(m_resourceSearchString)) {
            m_successfulTests |= RESOURCE_SUBSTRING;
            return;
        }
    }
}

