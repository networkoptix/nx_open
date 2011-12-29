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

    enum State {
        INITIAL,
        INVALID,
        STARTED,
        FINISHED
    };

    QnAutoTester(int &argc, char **argv, QObject *parent = NULL);

    virtual ~QnAutoTester();

    State state() const {
        return m_state;
    }

    const QString &message() const {
        return m_message;
    }

    bool succeeded() const {
        return m_succeeded;
    }

signals:
    void finished();

public slots:
    void start();

protected slots:
    void at_timer_timeout();

protected:
    bool needsTesting(Test test);

    void testResourceSubstring();

private:
    /** Current tester state. */
    State m_state;

    /** All tests that have to be performed. */
    Tests m_allTests;

    /** Tests that succeeded. */
    Tests m_successfulTests;

    /** Time when the auto tester has started, in milliseconds since epoch. */
    qint64 m_startTime;

    /** Auto tester timeout, in milliseconds. */
    qint64 m_timeout;

    /** String that must be present in resources. */
    QString m_resourceSearchString;

    /** Timer. */
    QTimer *m_timer;

    bool m_succeeded;

    QString m_message;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnAutoTester::Tests);

#endif // QN_AUTO_TESTER_H
