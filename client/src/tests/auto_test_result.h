#ifndef QN_AUTO_TEST_RESULT_H
#define QN_AUTO_TEST_RESULT_H

#include <utils/common/exception.h>

class QnAutoTestResult: public QnException {
public:
    QnAutoTestResult(bool success, const QString &message): QnException(message), m_success(success) {}

    bool succeeded() const {
        return m_success;
    }

private:
    bool m_success;
};

#endif // QN_AUTO_TEST_RESULT_H
