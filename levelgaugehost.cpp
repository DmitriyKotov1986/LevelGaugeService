#include "levelgaugehost.h"

//Qt
#include <QSqlQuery>

//My
#include "Common/common.h"
#include "Common/tdbloger.h"

using namespace LevelGaugeService;

using namespace Common;

LevelGaugeHost::LevelGaugeHost(QObject *parent)
    : QObject{parent}
{
    //настраиваем подключение к БД
    _db = QSqlDatabase::addDatabase(_cnf->db_Driver(), "MainDB");
    _db.setDatabaseName(_cnf->db_DBName());
    _db.setUserName(_cnf->db_UserName());
    _db.setPassword(_cnf->db_Password());
    _db.setConnectOptions(_cnf->db_ConnectOptions());
    _db.setPort(_cnf->db_Port());
    _db.setHostName(_cnf->db_Host());

    if (!_db.open())
    {
        _loger->sendLogMsg(TDBLoger::MSG_CODE::CRITICAL_CODE, QString("Cannot connect to database. Error: %1").arg(_db.lastError().text()));

        exit(EXIT_CODE::SQL_NOT_OPEN_DB);
    };
}

LevelGaugeHost::~LevelGaugeHost()
{
    if (_db.isOpen())
    {
        _db.close();
    }
}

QString LevelGaugeHost::errorString()
{
    auto res = _errorString;
    _errorString.clear();

    return res;
}

void LevelGaugeHost::start()
{
    Q_ASSERT(_db.isOpen());

    _db.transaction();

    QSqlQuery query(_db);
    query.setForwardOnly(true);

    //загружаем данные об АЗС
    QString queryText = QString("SELECT [AZSCode], [LevelGaugeServiceDB], [TimeShift] "
                                "FROM [AZSInfo] "
                                "WHERE [LevelGaugeServiceEnabled] = 1");

    if (!query.exec(queryText))
    {
        errorDBQuery(_db, query);
    }

    while (query.next())
    {
        TAZS tmp;
        tmp.DBName = Query.value("LevelGaugeServiceDB").toString();//имя БД для сохранения результатов
        tmp.TimeShift = Query.value("TimeShift").toInt(); //cмещение времени относительно сервера
        AZSS.insert(Query.value("AZSCode").toString(), tmp);
    }

}

void LevelGaugeHost::stop()
{

}
