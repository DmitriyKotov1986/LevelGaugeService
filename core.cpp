//QT
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QSqlError>

//My
#include "common/common.h"
#include "commondefines.h"

#include "core.h"


using namespace LevelGaugeService;
using namespace Common;

static const QString CORE_CONNECTION_TO_DB_NAME = "CoreDB";

Core::Core(QObject *parent)
    : QObject{parent}
    , _cnf(TConfig::config())
    , _loger(Common::TDBLoger::DBLoger())
{
    Q_CHECK_PTR(_cnf);
    Q_CHECK_PTR(_loger);
}

Core::~Core()
{
    Q_ASSERT(_tanksThread.size() != 0);
}

QString Core::errorString()
{
    auto res = _errorString;
    _errorString.clear();

    return res;
}

bool Core::startSync()
{
    auto syncThread = std::make_unique<SyncThread>();
    syncThread->sync = std::make_unique<Sync>();
    syncThread->thread = std::make_unique<QThread>();
    syncThread->sync->moveToThread(syncThread->thread.get());

    //запускаем обработку стазу после создания потока
    QObject::connect(syncThread->thread.get(), SIGNAL(started()), syncThread->sync.get(), SLOT(start()), Qt::QueuedConnection);

    //тормозим поток при отключении сервера
    QObject::connect(this, SIGNAL(stopAll()), syncThread->sync.get(), SLOT(stop()), Qt::QueuedConnection);

    //ставим поток на удаление когда он сам завершился
    QObject::connect(syncThread->sync.get(), SIGNAL(finished()), syncThread->thread.get(), SLOT(quit()));

    //запускаем поток на выполнение
    syncThread->thread->start(QThread::NormalPriority);

    return true;
}

void Core::start()
{
    //настраиваем подключение БД
    QSqlDatabase db;

    const auto& dbConnectionInfo = _cnf->dbConnectionInfo();
    db = QSqlDatabase::addDatabase(dbConnectionInfo.db_Driver, CORE_CONNECTION_TO_DB_NAME);
    db.setDatabaseName(dbConnectionInfo.db_DBName);
    db.setUserName(dbConnectionInfo.db_UserName);
    db.setPassword(dbConnectionInfo.db_Password);
    db.setConnectOptions(dbConnectionInfo.db_ConnectOptions);
    db.setPort(dbConnectionInfo.db_Port);
    db.setHostName(dbConnectionInfo.db_Host);

    //подключаемся к БД
    if (!db.open())
    {
        _errorString = connectDBErrorString(db);
        QSqlDatabase::removeDatabase(CORE_CONNECTION_TO_DB_NAME);

        return;
    };

    //загружаем конфигурацию
    db.transaction();
    QSqlQuery query(db);
    query.setForwardOnly(true);

    //загружаем данные об АЗС
    const auto queryText =
            QString("SELECT [AZSCode], [TankNumber] "
                    "FROM [TanksInfo] "
                    "WHERE A.[Enabled] <> 0");

    if (!query.exec(queryText))
    {
        _errorString = executeDBErrorString(db, query);

        db.rollback();
        db.close();
        QSqlDatabase::removeDatabase(CORE_CONNECTION_TO_DB_NAME);

        return;
    }

    while (query.next())
    {
        TankID id{query.value("AZSCode").toString(), static_cast<quint8>(query.value("TankNumber").toUInt())};
        auto tankThread = std::make_unique<TankThread>(); //tank удалиться при остановке потока
        tankThread->tank = std::make_unique<Tank>(id, dbConnectionInfo);
        tankThread->thread =  std::make_unique<QThread>();
        tankThread->tank->moveToThread(tankThread->thread.get());

        //запускаем обработку стазу после создания потока
        QObject::connect(tankThread->thread.get(), SIGNAL(started()), tankThread->tank.get(), SLOT(start()), Qt::QueuedConnection);

        //тормозим поток при отключении сервера
        QObject::connect(this, SIGNAL(stopAll()), tankThread->tank.get(), SLOT(stop()), Qt::QueuedConnection);

        //ставим поток на удаление когда он сам завершился
        QObject::connect(tankThread->tank.get(), SIGNAL(finished()), tankThread->thread.get(), SLOT(quit()));

        //запускаем поток на выполнение
        tankThread->thread->start(QThread::NormalPriority);

        _tanksThread.push_back(std::move(tankThread));
    }

    if (!db.commit())
    {
        _errorString = commitDBErrorString(db);

        db.rollback();
        db.close();

        QSqlDatabase::removeDatabase(CORE_CONNECTION_TO_DB_NAME);

        return;
    }

    db.close();
    QSqlDatabase::removeDatabase(CORE_CONNECTION_TO_DB_NAME);

    if (!startSync())
    {
        _errorString = QString("Cannot start sync");
    }
}

void Core::stop()
{
    emit stopAll();

    for (const auto& thread: _tanksThread)
    {
        Q_CHECK_PTR(thread->thread);

        if (thread->thread != nullptr)
        {
            thread->thread->wait();
        }
    }

    _tanksThread.clear();

    Q_CHECK_PTR(_syncThread->thread);
    if (_syncThread->thread != nullptr)
    {
        _syncThread->thread->wait();
    }
    _syncThread.reset(nullptr);
}
