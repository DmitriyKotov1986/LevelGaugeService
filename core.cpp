#include <QSqlQuery>
#include <QThread>

#include "Common/common.h"

#include "core.h"

using namespace LevelGaugeService;
using namespace Common;

Core::Core(QObject *parent)
    : QObject{parent}
    , _cnf(TConfig::config())
    , _loger(Common::TDBLoger::DBLoger())
{
    Q_CHECK_PTR(_cnf);
    Q_CHECK_PTR(_loger);

    //настраиваем подключение БД
    const auto& dbConnectionInfo = _cnf->dbConnectionInfo();
    _db = QSqlDatabase::addDatabase(dbConnectionInfo.db_Driver, "CoreDB");
    _db.setDatabaseName(dbConnectionInfo.db_DBName);
    _db.setUserName(dbConnectionInfo.db_UserName);
    _db.setPassword(dbConnectionInfo.db_Password);
    _db.setConnectOptions(dbConnectionInfo.db_ConnectOptions);
    _db.setPort(dbConnectionInfo.db_Port);
    _db.setHostName(dbConnectionInfo.db_Host);

    //подключаемся к БД
    if ((_db.isOpen()) || (!_db.open()))
    {
        _errorString = QString("Cannot connect to database. Error: %1").arg(_db.lastError().text());

        return;
    };
}

Core::~Core()
{
    if (_db.isOpen())
    {
        _db.close();
    }

    QSqlDatabase::removeDatabase("CoreDB");
}

QString Core::errorString()
{
    auto res = _errorString;
    _errorString.clear();

    return res;
}

void Core::loadTankConfig()
{
    Q_ASSERT(_db.isOpen());

    _db.transaction();
    QSqlQuery query(_db);
    query.setForwardOnly(true);

    //загружаем данные об АЗС
    QString queryText = "SELECT A.[AZSCode], [TankNumber], [Volume], [Diametr], [Product], [LastSaveDateTime], [LastIntakeDateTime], [TimeShift], [LevelGaugeServiceDB] "
                        "FROM [TanksInfo] AS A "
                        "INNER JOIN ( "
                        "   SELECT [AZSCode], [TimeShift], [LevelGaugeServiceDB] "
                        "   FROM [AZSInfo] WHERE [LevelGaugeServiceEnabled] = 1 "
                        ") AS B "
                        "ON A.[AZSCode] = B.[AZSCode] "
                        "WHERE A.[Enabled] = 1";

    if (!query.exec(queryText))
    {
         errorDBQuery(_db, query);
    }

    while (query.next())
    {
        Tank::TankConfig tmp;
        tmp.AZSCode = query.value("AZSCode").toString();
        tmp.tankNumber = query.value("TankNumber").toUInt();
        tmp.dbNitName = query.value("LevelGaugeServiceDB").toString();//имя БД для сохранения результатов
        tmp.lastIntake = query.value("LastIntakeDateTime").toDateTime();
        tmp.timeShift = query.value("TimeShift").toInt(); //cмещение времени относительно сервера
        tmp.product =  query.value("Productr").toString();
        tmp.diametr = query.value("Diametr").toFloat();
        tmp.volume = query.value("Volume").toFloat();

        _tanks.push_back(std::move(tmp));
    }

    DBCommit(_db);
}

void Core::start()
{
    //загружаем конфигурацию
    loadTankConfig();

    //создаем и запускаем потоки для каждого из резервуаров
    for (const auto& tankConfig: _tanks)
    {
        auto tank = new Tank(tankConfig); //tank удалиться при остановке потока
        auto thread = new QThread();
        tank->moveToThread(thread);

        //запускаем обработку стазу после создания потока
        QObject::connect(thread, SIGNAL(started()), tank, SLOT(start()));

        //тормозим поток при отключении сервера
        QObject::connect(this, SIGNAL(stopAll()), thread, SLOT(quit()));

        //ставим поток на удаление когда он сам завершился
        QObject::connect(thread, SIGNAL(finished()), tank, SLOT(deleteLater()));

        //ставим обработчик на удаление когда он завершился
        QObject::connect(thread, SIGNAL(finished()), thread, SLOT(deleteLater()));

        //запускаем поток на выполнение
        thread->start(QThread::NormalPriority);
    }
}

void Core::stop()
{
    emit stopAll();
}
