#ifndef QN_AUTO_TESTER_H
#define QN_AUTO_TESTER_H

#include <QtCore/QObject>

class QTimer;

class QnAutoTester: public QObject {
    Q_OBJECT;
    Q_FLAGS(Tests Test);
    Q_ENUMS(State);

public:
    // TODO: #Elric #enum
    enum Test {
        ResourceSubstring = 0x1 /**< Test for the presence of a substring in resource names. */
    };
    Q_DECLARE_FLAGS(Tests, Test);

    enum State {
        Initial,    /**< Ready for testing. */
        Invalid,    /**< There was an error in command line arguments. Error description was printed to STDERR. */
        Running,    /**< Testing. */
        Finished    /**< Testing finished, results available. */
    };

    QnAutoTester(int &argc, char **argv, QObject *parent = NULL);

    virtual ~QnAutoTester();

    /**
     * \returns                         Current state of the auto tester.
     */
    State state() const {
        return m_state;
    }

    /**
     * \returns                         All tests that this auto tester has to perform.
     */
    Tests tests() const {
        return m_allTests;
    }

    /**
     * \returns                         Textual description of the testing results.
     */
    const QString &message() const {
        return m_message;
    }

    /**
     * \returns                         Whether all tests were successful. 
     */
    bool succeeded() const {
        return m_succeeded;
    }

signals:
    /**
     * This signal is emitted when testing is finished.
     */
    void finished();

public slots:
    /**
     * Starts testing. Note that if there are no tests to perform, testing will
     * be finished immediately.
     */
    void start();

protected slots:
    void at_timer_timeout();

protected:
    bool needsTesting(Test test);

    void testResourceSubstring();

private:
    /** Current tester state. */
    State m_state;

    /** Time when the auto tester was started, in milliseconds since epoch. */
    qint64 m_startTime;

    /** Auto tester timeout, in milliseconds. */
    qint64 m_timeout;

    /** All tests that have to be performed. */
    Tests m_allTests;

    /** Tests that succeeded. */
    Tests m_successfulTests;

    /** Timer used for testing. */
    QTimer *m_timer;

    /** Whether all tests succeeded. */
    bool m_succeeded;

    /** String that must be present in resources. */
    QString m_resourceSearchString;

    /** Textual description of the testing results. */
    QString m_message;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnAutoTester::Tests);

#endif // QN_AUTO_TESTER_H
