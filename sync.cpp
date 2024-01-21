//My
#include "Common/common.h"

#include "sync.h"

using namespace LevelGaugeService;
using namespace Common;

static const QString CONNECTION_TO_DB_NAME = "Sync";
static const QString CONNECTION_TO_DB_NIT_NAME = "SyncNIT";

Sync::Sync(QObject *parent)
    : QObject{parent}
    , _cnf(TConfig::config())
{
    Q_CHECK_PTR(_cnf);
}

Sync::~Sync()
{
    stop();
}

void Sync::start()
{
    if (!connectToDB(_db, _cnf->dbConnectionInfo(), CONNECTION_TO_DB_NAME))
    {
        emit errorOccurred(connectDBErrorString(_db));

        return;
    }

    if (!connectToDB(_dbNit, _cnf->dbNitConnectionInfo(), CONNECTION_TO_DB_NIT_NAME))
    {
        _db.close();

        emit errorOccurred(connectDBErrorString(_db));

        return;
    }

    Q_ASSERT(_timer == nullptr);
    _timer = new QTimer();

    QObject::connect(_timer, SIGNAL(timeout()), SLOT(sync()));

    _timer->start(60000);
}

void Sync::stop()
{
    delete _timer;
    _timer = nullptr;

    if (_db.isOpen())
    {
        _db.close();
    }
    if (_dbNit.isOpen())
    {
        _dbNit.close();
    }

    QSqlDatabase::removeDatabase(CONNECTION_TO_DB_NAME);
    QSqlDatabase::removeDatabase(CONNECTION_TO_DB_NIT_NAME);

    emit finished();
}

void Sync::addStatusForSync(const TankID &id, const QDateTime dateTime, const TankStatus &tankStatus)
{
    StatusData tmp{id, dateTime, tankStatus};

    _statusesData.push(std::move(tmp));
}

void Sync::sync()
{
    while (!_statusesData.empty())
    {
        const auto statusData = std::move(_statusesData.front());
        _statusesData.pop();

        const auto tanksConfig_it = _tanksConfig.find(statusData.id);
        if (tanksConfig_it == _tanksConfig.end())
        {
            Q_ASSERT(false);

            emit errorOccurred("Internal error: Sync::saveToDBNit undefine tankID");

            return;
        }
        const auto& tankConfig = tanksConfig_it.value();
        const auto& id =  tanksConfig_it.key();

        Q_ASSERT(!tankConfig.dbNitName.isEmpty());
        Q_ASSERT(id.tankNumber != 0);
        Q_ASSERT(!id.levelGaugeCode.isEmpty());

        switch (tankConfig.mode)
        {
        case Mode::AZS:
        {
            saveToDBNitAZS(statusData, id, tankConfig);

            break;
        }
        case Mode::OIL_DEPOT:
        {
            saveToDBNitOilDepot(statusData, id, tankConfig);

            break;
        }
        default: Q_ASSERT(false);
        }

        const quint64 saveId = getLastSavetId(tankConfig);
        saveToDB(statusData, id, tankConfig, saveId);
    }
}

void Sync::saveToDB(const StatusData& statusData, const TankID& id, const TankConfig& tankConfig, quint64 recordID) const
{
    //Сохряняем в БД
    const auto queryText =
        QString("INSERT INTO [dbo].[TanksCalculate] "
                    "([AZSCode], [TankNumber], [DateTime], "
                    "[Volume], [TotalVolume], [Mass], [Density], [Height], [Temp], [Product], [ProductStatus], "
                    "[TankName], [ServiceID], [ServiceDB], [Type], [AdditionFlag], [Status], [Mode], [SaveDateTime]) "
                "VALUES ("
                    "'%1', %2, CAST('%3' AS DATETIME2), "
                    "%4, %5, %6, %7, %8, %9, '%10', %11, "
                    "'%12', %13, '%14', %15, %16, %17, %18, CAST('%19' AS DATETIME2))")
            .arg(id.levelGaugeCode)
            .arg(id.tankNumber)
            .arg(statusData.dateTime.addSecs(tankConfig.timeShift).toString(DATETIME_FORMAT))  //переводим время на время АЗС
            .arg(statusData.tankStatus.volume / 1000.0, 0, 'f', 0) //переводим объем в м3
            .arg(tankConfig.totalVolume / 1000.0, 0, 'f', 0)   //переводим объем в м3
            .arg(statusData.tankStatus.mass, 0, 'f', 0)
            .arg(statusData.tankStatus.density, 0, 'f', 1)
            .arg(statusData.tankStatus.height / 10.0, 0, 'f', 1)   //высоту переводм в см
            .arg(statusData.tankStatus.temp, 0, 'f', 1)
            .arg(tankConfig.product)
            .arg(static_cast<quint8>(tankConfig.productStatus))
            .arg(tankConfig.tankName)
            .arg(recordID)
            .arg(tankConfig.dbNitName)
            .arg(static_cast<quint8>(tankConfig.type))
            .arg(static_cast<quint8>(statusData.tankStatus.flag))
            .arg(static_cast<quint8>(statusData.tankStatus.status))
            .arg(static_cast<quint8>(tankConfig.mode))
            .arg(QDateTime::currentDateTime().toString(DATETIME_FORMAT));

    executeDBQuery(_db, queryText);
}

void Sync::saveLastDateTime(const StatusData &statusData, const TankID &id, const TankConfig& tankConfig) const
{
    const auto queryText =
        QString("UPDATE [dbo].[TanksInfo] "
                "SET [LastSaveDateTime] = CAST('%1' AS DATETIME2) "
                "WHERE [AZSCode] = '%2' AND [TankNumber] = %3 ")
            .arg(statusData.dateTime.addSecs(tankConfig.timeShift).toString(DATETIME_FORMAT))  //переводим время на время АЗС
            .arg(id.levelGaugeCode)
            .arg(id.tankNumber);

    executeDBQuery(_db, queryText);
}

void Sync::saveToDBNitOilDepot(const StatusData& statusData, const TankID& id, const TankConfig& tankConfig) const
{
    Q_ASSERT(!tankConfig.dbNitName.isEmpty());

    //Сохряняем в БД
    const auto queryText =
        QString("INSERT INTO [%1].[dbo].[TanksStatus] ("
                    "[DateTime], [AZSCode], [TankNumber], [TankName], [Type], [Mode], [Product], "
                    "[ProductStatus], [Height], [Volume], [TotalVolume], [Temp], [Density], [Mass]) "
                "VALUES (CAST('%2' AS DATETIME2), '%3', %4, '%5', %6, %7, %8, "
                    "%9, %10, %11, %12, %13, %14, %15)")
            .arg(tankConfig.dbNitName)
            .arg(statusData.dateTime.addSecs(tankConfig.timeShift).toString(DATETIME_FORMAT))  //переводим время на время АЗС
            .arg(id.levelGaugeCode)
            .arg(id.tankNumber)
            .arg(tankConfig.tankName)
            .arg(static_cast<quint8>(tankConfig.type))
            .arg(static_cast<quint8>(tankConfig.mode))
            .arg(tankConfig.product)
            .arg(static_cast<quint8>(tankConfig.productStatus))
            .arg(statusData.tankStatus.height / 10.0, 0, 'f', 1)   //высоту переводм в см
            .arg(statusData.tankStatus.volume / 1000.0, 0, 'f', 0) //переводим объем в м3
            .arg(tankConfig.totalVolume / 1000.0, 0, 'f', 0)   //переводим объем в м3
            .arg(statusData.tankStatus.temp, 0, 'f', 1)
            .arg(statusData.tankStatus.density, 0, 'f', 1)
            .arg(statusData.tankStatus.mass, 0, 'f', 0);

    executeDBQuery(_dbNit, queryText);
}

void Sync::saveToDBNitAZS(const StatusData &statusData, const TankID &id, const TankConfig &tankConfig) const
{
    Q_ASSERT(!tankConfig.dbNitName.isEmpty());

    const auto queryText =
        QString("INSERT INTO [%1].[dbo].[TanksStatus] ([DateTime], [AZSCode], [TankNumber], [Product], [Height], [Volume], [Temp], [Density], [Mass]) "
                "VALUES (CAST('%2' AS DATETIME2), '%3', %4, '%5', %6, %7, %8, %9, %10)")
            .arg(tankConfig.dbNitName)
            .arg(statusData.dateTime.addSecs(tankConfig.timeShift).toString(DATETIME_FORMAT))  //переводим время на время АЗС
            .arg(id.levelGaugeCode)
            .arg(id.tankNumber)
            .arg(tankConfig.product)
            .arg(statusData.tankStatus.height / 10.0, 0, 'f', 1)  //высоту переводм в см
            .arg(statusData.tankStatus.volume, 0, 'f', 0)
            .arg(statusData.tankStatus.temp, 0, 'f', 1)
            .arg(statusData.tankStatus.density, 0, 'f', 1)
            .arg(statusData.tankStatus.mass, 0, 'f', 0);

    executeDBQuery(_dbNit, queryText);
}

quint64 Sync::getLastSavetId(const TankConfig &tankConfig) const
{
    Q_ASSERT(_dbNit.isOpen());
    Q_ASSERT(!tankConfig.dbNitName.isEmpty());

    quint64 result = 0;

    //Сохряняем в БД
    const auto queryText =
        QString("SELECT TOP(1) [ID] "
                "FROM [dbo].%1 "
                "ORDER BY [ID] DESC ")
            .arg(tankConfig.dbNitName);

    _dbNit.transaction();
    QSqlQuery query(_dbNit);

    if (!query.exec(queryText))
    {
        emit errorOccurred(executeDBErrorString(_dbNit, query));
        _dbNit.rollback();

        return result;
    }

    if (query.next())
    {
        result = query.value("ID").toULongLong();
    }

    if (!_dbNit.commit())
    {
        emit errorOccurred(commitDBErrorString(_dbNit));
        _dbNit.rollback();

        return result;
    }

    return result;
}

void Sync::executeDBQuery(QSqlDatabase &db, const QString& queryText) const
{
    Q_ASSERT(db.isOpen());

    db.transaction();
    QSqlQuery query(db);

    if (!query.exec(queryText))
    {
        emit errorOccurred(executeDBErrorString(db, query));

        _dbNit.rollback();
    }

    if (!db.commit())
    {
        emit errorOccurred(commitDBErrorString(db));

        db.rollback();
    }
}
