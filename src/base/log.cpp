#include "log.h"

#include <QMutexLocker>
#include <QDateTime>
#include <QThread>

CLLog cl_log;

QString cl_log_msg[] =  { "ALWAYS", "ERROR", "WARNING", "INFO", "DEBUG1", "DEBUG2" };

CLLog::CLLog():
m_loglistner(0)
{

}

bool CLLog::create(const QString& base_name,
				   quint32 max_file_size, 
					quint8 maxBackupFiles, 
					CLLogLevel loglevel )
{
	m_base_name = base_name;
	m_max_file_size = max_file_size;
	m_maxBackupFiles = maxBackupFiles;
	m_cur_num = 0;
	
	m_loglevel = loglevel;

	m_file.setFileName(currFileName());

	return m_file.open(QIODevice::WriteOnly | QIODevice::Append);

}

void CLLog::setLogLevel(CLLogLevel loglevel)
{
	m_loglevel = loglevel;
}


void CLLog::setLoglistner(CLLogListner * loglistner)
{
	m_loglistner = loglistner;
}


void CLLog::log(const QString& msg, int val, CLLogLevel loglevel)
{
	if (loglevel > m_loglevel)
		return;

	QString str;
	QTextStream(&str) << msg << val;

	log(str, loglevel);
}

void CLLog::log(const QString& msg1, const QString& msg2, CLLogLevel loglevel)
{
	if (loglevel > m_loglevel)
		return;

	QString str;
	QTextStream(&str) << msg1 << msg2;

	log(str, loglevel);
}

void CLLog::log(const QString& msg1, int val, const QString& msg2, CLLogLevel loglevel)
{
	if (loglevel > m_loglevel)
		return;

	QString str;
	QTextStream(&str) << msg1 << val << msg2;

	log(str, loglevel);

}

void CLLog::log(const QString& msg, float val, CLLogLevel loglevel)
{
	if (loglevel > m_loglevel)
		return;

	QString str;
	QTextStream(&str) << msg << val;

	log(str, loglevel);

}

void CLLog::log(const QString& msg, CLLogLevel loglevel)
{
	return;

	if (loglevel > m_loglevel)
		return;

	QMutexLocker mutx(&m_mutex);

	if (m_loglistner)
		m_loglistner->onLogMsg(msg);

	if (!m_file.isOpen())
		return;

	
	QString th;
	hex(QTextStream (&th)) <<(quint32)QThread::currentThread();

	QTextStream fstr(&m_file);
	fstr<< QDateTime::currentDateTime().toString("ddd MMM d yy  hh:mm:ss.zzz") <<	" Thread " << th << " (" <<cl_log_msg[loglevel] <<"): " << msg << "\r\n";
	fstr.flush();


	if (m_file.size() >= m_max_file_size) 
		openNextFile();


}

void CLLog::openNextFile()
{
	m_file.close(); // close current file

	int max_existing_num = m_maxBackupFiles;
	while (max_existing_num)
	{
		if (QFile::exists(backupFileName(max_existing_num)))
			break;
		--max_existing_num;
	}

	if (max_existing_num<m_maxBackupFiles) // if we do not need to delete backupfiles
	{
		QFile::rename(currFileName(), backupFileName(max_existing_num+1));
	}
	else // we need to delete one backup file
	{
		QFile::remove(backupFileName(1)); // delete the oldest file

		for (int i = 2; i <= m_maxBackupFiles; ++i) // shift all file names by one 
			QFile::rename(backupFileName(i), backupFileName(i-1));

		QFile::rename(currFileName(), backupFileName(m_maxBackupFiles)); // move current to the most latest backup
	}

	m_file.open(QIODevice::WriteOnly | QIODevice::Append);

}


QString CLLog::currFileName() const
{
	return m_base_name + ".log";
}

QString CLLog::backupFileName(quint8 num) const
{
	QString result;
	QTextStream stream(&result);

	char cnum[4];
	sprintf(cnum, "%.3d", num);

	stream << m_base_name << cnum << ".log";
	return result;
}

