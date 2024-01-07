//QT
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QSqlError>

//My
#include "Common/common.h"

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
        _errorString = QString("Cannot connect to database. Error: %1").arg(db.lastError().text());
        QSqlDatabase::removeDatabase(CORE_CONNECTION_TO_DB_NAME);

        return;
    };

    //загружаем конфигурацию
    db.transaction();
    QSqlQuery query(db);
    query.setForwardOnly(true);

    //загружаем данные об АЗС
    const auto queryText =
            QString("SELECT [ID] "
                    "FROM [TanksInfo] "
                    "WHERE A.[Enabled] <> 0");

    if (!query.exec(queryText))
    {
        _errorString = QString("Cannot execute query. Error: %1. Query: %2").arg(query.lastError().text()).arg(query.lastQuery());

        db.rollback();
        db.close();
        QSqlDatabase::removeDatabase(CORE_CONNECTION_TO_DB_NAME);

        return;
    }

    while (query.next())
    {
        qint64 id = query.value("ID").toLongLong();
        auto tankThread = std::make_unique<TankThread>(); //tank удалиться при остановке потока
        tankThread->tank = std::make_unique<Tank>(id, dbConnectionInfo);
        tankThread->thread =  std::make_unique<QThread>();
        tankThread->tank->moveToThread(tankThread->thread.get());

        //запускаем обработку стазу после создания потока
        QObject::connect(tankThread->thread.get(), SIGNAL(started()), tankThread->tank.get(), SLOT(start()));

        //тормозим поток при отключении сервера
        QObject::connect(this, SIGNAL(stopAll()), tankThread->tank.get(), SLOT(stop()));

        //ставим поток на удаление когда он сам завершился
        QObject::connect(tankThread->tank.get(), SIGNAL(finished()), tankThread->thread.get(), SLOT(quit()));

        //запускаем поток на выполнение
        tankThread->thread->start(QThread::NormalPriority);

        _tanksThread.push_back(std::move(tankThread));
    }

    if (!db.commit())
    {
        _errorString = QString("Cannot commit trancsation. Error: %1").arg(db.lastError().text());
        db.rollback();
        db.close();
        QSqlDatabase::removeDatabase(CORE_CONNECTION_TO_DB_NAME);

        return;
    }

    db.close();
    QSqlDatabase::removeDatabase(CORE_CONNECTION_TO_DB_NAME);

}

void Core::stop()
{
    emit stopAll();

    for (const auto& thread: _tanksThread)
    {
        thread->thread->wait();
    }

    _tanksThread.clear();
}
