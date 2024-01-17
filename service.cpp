//Qt
#include <QFileInfo>
#include <QDir>

//My
#include "Common/common.h"
#include "Common/tdbloger.h"

#include "service.h"
#include "core.h"

using namespace LevelGaugeService;
using namespace Common;

Service::Service(int argc, char **argv)
    : QtService<QCoreApplication>(argc, argv, QCoreApplication::applicationName())
    , _cnf(TConfig::config())

{
    Q_CHECK_PTR(_cnf);

    setServiceDescription("LevelGauge Service");
    setServiceFlags(QtServiceBase::CanBeSuspended);

    //настраиваем подключение БД логирования
    const auto& dbConnectionInfo = _cnf->dbConnectionInfo();
    auto logdb = QSqlDatabase::addDatabase(dbConnectionInfo.db_Driver, "LoglogDB");
    logdb.setDatabaseName(dbConnectionInfo.db_DBName);
    logdb.setUserName(dbConnectionInfo.db_UserName);
    logdb.setPassword(dbConnectionInfo.db_Password);
    logdb.setConnectOptions(dbConnectionInfo.db_ConnectOptions);
    logdb.setPort(dbConnectionInfo.db_Port);
    logdb.setHostName(dbConnectionInfo.db_Host);

    _loger = Common::TDBLoger::DBLoger(&logdb, _cnf->sys_DebugMode(), "LevelGaugeServiceLog");
    if (_loger->isError())
    {
        QString msg = QString("Loger initialization error. Error: %1").arg(_loger->errorString());
        qCritical() << QString("%1 %2").arg(QTime::currentTime().toString("hh:mm:ss")).arg(msg);
        writeLogFile("ERR>", msg);

        exit(EXIT_CODE::START_LOGGER_ERR); // -1
    }
}

Service::~Service()
{
    if (_isRun)
    {
        this->stop();
    }

    if (_loger != nullptr)
    {
        _loger->sendLogMsg(TDBLoger::MSG_CODE::OK_CODE, "Successfully finished");

        TDBLoger::deleteDBLoger();
    }
}

QString Service::errorString()
{
    auto res = _errorString;
    _errorString.clear();

    return res;
}

void Service::start()
{
    Q_CHECK_PTR(_cnf);
    Q_CHECK_PTR(_loger);

    if (_isRun)
    {
        _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, "Service already run. Start ignored");

        return;
    }

    _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, "Service start");

    try
    {
        _core = new Core();
        _core->start();

        if (_core->isError())
        {
            throw std::runtime_error(_core->errorString().toStdString());
        }

        _isRun = true;
    }
    catch (const std::exception &e)
    {
        _loger->sendLogMsg(TDBLoger::MSG_CODE::CRITICAL_CODE, QString("Critical error start service: %1").arg(e.what()));

        exit(EXIT_CODE::SERVICE_START_ERR);
    }
}

void Service::pause()
{
    stop();
}

void Service::resume()
{
    start();
}

void Service::stop()
{
    Q_CHECK_PTR(_loger);

    if (_loger == nullptr)
    {
        return;
    }

    if (!_isRun)
    {
        _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, "Service is not runing. Stop ignored");

        return;
    }

    _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, "Service stop");

    Q_CHECK_PTR(_core);

    try
    {
        _core->stop();

        if (_core->isError())
        {
            throw std::runtime_error(_core->errorString().toStdString());
        }

        delete _core;

        _core = nullptr;
    }
    catch (const std::exception &e)
    {
        _loger->sendLogMsg(TDBLoger::MSG_CODE::CRITICAL_CODE, QString("Critical error stop service: %1").arg(e.what()));

        exit(EXIT_CODE::SERVICE_STOP_ERR);
    }
}




