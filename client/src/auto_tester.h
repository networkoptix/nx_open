#ifndef QN_AUTO_TESTER_H
#define QN_AUTO_TESTER_H

#include <QObject>

class QTimer;

class QnAutoTester: public QObject {
    Q_OBJECT;
public:
    enum Test {
        RESOURCE_SUBSTRING = 0x1
    };
    Q_DECLARE_FLAGS(Tests, Test);

    QnAutoTester(int &argc, char **argv, QObject *parent = NULL);

    virtual ~QnAutoTester();

    void start();

protected slots:
    void at_timer_timeout();

protected:
    bool needsTesting(Test test);

    void testResourceSubstring();

private:
    /** Whether the command line was OK. */
    bool m_valid;

    /** Whether the tester was started. */
    bool m_started;

    Tests m_allTests;

    Tests m_successfulTests;

    /** Time when the auto tester has started, in milliseconds since epoch. */
    qint64 m_startTime;

    /** Auto tester timeout, in milliseconds. */
    qint64 m_timeout;

    /** String that must be present in resources. */
    QString m_resourceSearchString;

    /** Timer. */
    QTimer *m_timer;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnAutoTester::Tests);

#endif // QN_AUTO_TESTER_H
