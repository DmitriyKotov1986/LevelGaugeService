//Qt
#include <QFileInfo>
#include <QDir>

//My
#include "Common/common.h"
#include "Common/tdbloger.h"
#include "core.h"

#include "service.h"

using namespace LevelGaugeService;
using namespace Common;

Service::Service(int& argc, char **argv)
    : QObject{nullptr}
    , QtService<QCoreApplication>(argc, argv, "LevelGaugeService")
    , _cnf(TConfig::config())
    , _loger(Common::TDBLoger::DBLoger())

{
    Q_CHECK_PTR(_cnf);
    Q_CHECK_PTR(_loger);

    setServiceDescription("LevelGaugeService");
    setServiceFlags(QtServiceBase::CanBeSuspended);

    QObject::connect(_loger, SIGNAL(errorOccurred(Common::EXIT_CODE, const QString&)),
                     SLOT(errorOccurredLoger(Common::EXIT_CODE, const QString&)), Qt::DirectConnection);

}

Service::~Service()
{
    if (_isRun)
    {
        this->stop();
    }

    if (_loger != nullptr)
    {
        _loger->sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, "Successfully finished");

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

        QObject::connect(_core, SIGNAL(errorOccurred(Common::EXIT_CODE, const QString&)),
                         SLOT(errorOccurredCore(Common::EXIT_CODE, const QString&)));

        _core->start();

        _isRun = true;
    }
    catch (const std::exception &e)
    {
        _errorString = QString("Critical error start service: %1").arg(e.what());
        _loger->sendLogMsg(TDBLoger::MSG_CODE::CRITICAL_CODE, _errorString);

        QCoreApplication::exit(EXIT_CODE::SERVICE_START_ERR);
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

        delete _core;

        _core = nullptr;
    }
    catch (const std::exception &e)
    {
        _errorString = QString("Critical error stop service: %1").arg(e.what());
        _loger->sendLogMsg(TDBLoger::MSG_CODE::CRITICAL_CODE, _errorString);

        QCoreApplication::exit(EXIT_CODE::SERVICE_STOP_ERR);
    }
}

void Service::errorOccurredLoger(Common::EXIT_CODE errorCode, const QString &errorString)
{
    QString msg = QString("Critical error while the loger is running. Code: %1 Message: %2").arg(errorCode).arg(errorString);
    qCritical() << msg;

    QCoreApplication::exit(errorCode);
}

void Service::errorOccurredCore(Common::EXIT_CODE errorCode, const QString &errorString)
{
    QString msg = QString("Critical error while the core is running. Code: %1 Message: %2").arg(errorCode).arg(errorString);
    _loger->sendLogMsg(Common::TDBLoger::MSG_CODE::CRITICAL_CODE, msg);

    QCoreApplication::exit(errorCode);
}




