//My
#include "Common/common.h"

#include "sync.h"

using namespace LevelGaugeService;
using namespace Common;

static const QString CONNECTION_TO_DB_NAME = "Sync";
static const QString CONNECTION_TO_DB_NIT_NAME = "SyncNIT";

static const int DETECT_STATUS_TIME = 60 * 15; //Время запаздывания записи данных в БД АОН НИТ

Sync::Sync(const Common::DBConnectionInfo& dbConnectionInfo, const Common::DBConnectionInfo& dbNitConnectionInfo,
           LevelGaugeService::TanksConfig* tanksConfig, QObject *parent /* = nullptr */)
    : QObject{parent}
    , _dbConnectionInfo(dbConnectionInfo)
    , _dbNitConnectionInfo(dbNitConnectionInfo)
{
    Q_CHECK_PTR(tanksConfig);
}

Sync::~Sync()
{
    stop();
}

void Sync::start()
{
    if (!connectToDB(_db, _dbConnectionInfo, CONNECTION_TO_DB_NAME))
    {
        emit errorOccurred(EXIT_CODE::SQL_NOT_CONNECT, connectDBErrorString(_db));

        return;
    }

    if (!connectToDB(_dbNit, _dbNitConnectionInfo, CONNECTION_TO_DB_NIT_NAME))
    {
        _db.close();

        emit errorOccurred(EXIT_CODE::SQL_NOT_CONNECT, connectDBErrorString(_db));

        return;
    }
}

void Sync::stop()
{
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

void Sync::calculateStatuses(const TankID &id, const TankStatusesList &tankStatuses)
{
    Q_ASSERT(tankStatuses.isEmpty());

    auto tankConfig = _tanksConfig->getTankConfig(id);
    Q_CHECK_PTR(tankConfig);

    QDateTime lastStatus;

    for(const auto& status: tankStatuses)
    {
        switch (tankConfig->mode())
        {
        case TankConfig::Mode::AZS:
        {
            saveToDBNitAZS(id, status);

            break;
        }
        case TankConfig::Mode::OIL_DEPOT:
        {
            saveToDBNitOilDepot(id, status);

            break;
        }
        default: Q_ASSERT(false);
        }

        const quint64 saveId = getLastSavetId(tankConfig->tankId());

        saveToDB(id, status, saveId);

        lastStatus = std::max(lastStatus, status.dateTime());
    }

    tankConfig->setLastSave(lastStatus);
}

void Sync::saveToDB(const LevelGaugeService::TankID& id, const LevelGaugeService::TankStatus& tankStatus, quint64 recordID)
{
    const auto tankConfig = _tanksConfig->getTankConfig(id);
    Q_CHECK_PTR(tankConfig);

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
            .arg(id.levelGaugeCode())
            .arg(id.tankNumber())
            .arg(tankStatus.dateTime().addSecs(tankConfig->timeShift()).toString(DATETIME_FORMAT))  //переводим время на время АЗС
            .arg(tankStatus.volume() / 1000.0, 0, 'f', 0) //переводим объем в м3
            .arg(tankConfig->totalVolume() / 1000.0, 0, 'f', 0)   //переводим объем в м3
            .arg(tankStatus.mass(), 0, 'f', 0)
            .arg(tankStatus.density(), 0, 'f', 1)
            .arg(tankStatus.height() / 10.0, 0, 'f', 1)   //высоту переводм в см
            .arg(tankStatus.temp(), 0, 'f', 1)
            .arg(tankConfig->product())
            .arg(static_cast<quint8>(tankConfig->productStatus()))
            .arg(tankConfig->name())
            .arg(recordID)
            .arg(tankConfig->serviceDB())
            .arg(static_cast<quint8>(tankConfig->type()))
            .arg(static_cast<quint8>(tankStatus.additionFlag()))
            .arg(static_cast<quint8>(tankStatus.status()))
            .arg(static_cast<quint8>(tankConfig->mode()))
            .arg(QDateTime::currentDateTime().toString(DATETIME_FORMAT));

    executeDBQuery(_db, queryText);
}

void Sync::saveToDBNitOilDepot(const LevelGaugeService::TankID& id, const LevelGaugeService::TankStatus& tankStatus)
{
    const auto tankConfig = _tanksConfig->getTankConfig(id);
    Q_CHECK_PTR(tankConfig);

    //Сохряняем в БД
    const auto queryText =
        QString("INSERT INTO [%1].[dbo].[TanksStatus] ("
                    "[DateTime], [AZSCode], [TankNumber], [TankName], [Type], [Mode], [Product], "
                    "[ProductStatus], [Height], [Volume], [TotalVolume], [Temp], [Density], [Mass]) "
                "VALUES (CAST('%2' AS DATETIME2), '%3', %4, '%5', %6, %7, %8, "
                    "%9, %10, %11, %12, %13, %14, %15)")
            .arg(tankConfig->serviceDB())
            .arg(tankStatus.dateTime().addSecs(tankConfig->timeShift()).toString(DATETIME_FORMAT))  //переводим время на время АЗС
            .arg(id.levelGaugeCode())
            .arg(id.tankNumber())
            .arg(tankConfig->name())
            .arg(static_cast<quint8>(tankConfig->type()))
            .arg(static_cast<quint8>(tankConfig->mode()))
            .arg(tankConfig->product())
            .arg(static_cast<quint8>(tankConfig->productStatus()))
            .arg(tankStatus.height() / 10.0, 0, 'f', 1)   //высоту переводм в см
            .arg(tankStatus.volume() / 1000.0, 0, 'f', 0) //переводим объем в м3
            .arg(tankConfig->totalVolume() / 1000.0, 0, 'f', 0)   //переводим объем в м3
            .arg(tankStatus.temp(), 0, 'f', 1)
            .arg(tankStatus.density(), 0, 'f', 1)
            .arg(tankStatus.mass(), 0, 'f', 0);

    executeDBQuery(_dbNit, queryText);
}

void Sync::saveToDBNitAZS(const LevelGaugeService::TankID& id, const LevelGaugeService::TankStatus& tankStatus)
{
    const auto tankConfig = _tanksConfig->getTankConfig(id);
    Q_CHECK_PTR(tankConfig);

    const auto queryText =
        QString("INSERT INTO [%1].[dbo].[TanksStatus] ([DateTime], [AZSCode], [TankNumber], [Product], [Height], [Volume], [Temp], [Density], [Mass]) "
                "VALUES (CAST('%2' AS DATETIME2), '%3', %4, '%5', %6, %7, %8, %9, %10)")
            .arg(tankConfig->serviceDB())
            .arg(tankStatus.dateTime().addSecs(tankConfig->timeShift()).toString(DATETIME_FORMAT))  //переводим время на время АЗС
            .arg(id.levelGaugeCode())
            .arg(id.tankNumber())
            .arg(tankConfig->product())
            .arg(tankStatus.height() / 10.0, 0, 'f', 1)  //высоту переводм в см
            .arg(tankStatus.volume(), 0, 'f', 0)
            .arg(tankStatus.temp(), 0, 'f', 1)
            .arg(tankStatus.density(), 0, 'f', 1)
            .arg(tankStatus.mass(), 0, 'f', 0);

    executeDBQuery(_dbNit, queryText);
}

quint64 Sync::getLastSavetId(const LevelGaugeService::TankID& id)
{
    Q_ASSERT(_dbNit.isOpen());

    const auto tankConfig = _tanksConfig->getTankConfig(id);
    Q_CHECK_PTR(tankConfig);

    quint64 result = 0;

    //Сохряняем в БД
    const auto queryText =
        QString("SELECT TOP(1) [ID] "
                "FROM [dbo].%1 "
                "ORDER BY [ID] DESC ")
            .arg(tankConfig->serviceDB());

    _dbNit.transaction();
    QSqlQuery query(_dbNit);

    if (!query.exec(queryText))
    {
        _dbNit.rollback();

        emit errorOccurred(EXIT_CODE::SQL_EXECUTE_QUERY_ERR, executeDBErrorString(_dbNit, query));

        return result;
    }

    if (query.next())
    {
        result = query.value("ID").toULongLong();
    }

    if (!_dbNit.commit())
    {
        _dbNit.rollback();

        emit errorOccurred(EXIT_CODE::SQL_COMMIT_ERR, commitDBErrorString(_dbNit));

        return result;
    }

    return result;
}

void Sync::executeDBQuery(QSqlDatabase &db, const QString& queryText)
{
    Q_ASSERT(db.isOpen());

    db.transaction();
    QSqlQuery query(db);

    if (!query.exec(queryText))
    {
        emit errorOccurred(EXIT_CODE::SQL_EXECUTE_QUERY_ERR, executeDBErrorString(db, query));

        _dbNit.rollback();
    }

    if (!db.commit())
    {
        emit errorOccurred(EXIT_CODE::SQL_COMMIT_ERR, commitDBErrorString(db));

        db.rollback();
    }
}

void Sync::calculateIntakes(const TankID &id, const IntakesList &intakes)
{
    Q_ASSERT(!intakes.empty());

    saveIntakesToNIT(id, intakes);
    saveIntakes(id, intakes);

    QDateTime lastIntake;
    for (const auto& intake: intakes)
    {
        lastIntake = std::max(lastIntake, intake.finishTankStatus().dateTime());
    }

    auto tankConfig = _tanksConfig->getTankConfig(id);
    Q_CHECK_PTR(tankConfig);

    tankConfig->setLastIntake(lastIntake);
}

void Sync::saveIntakesToNIT(const LevelGaugeService::TankID& id, const LevelGaugeService::IntakesList& intakes)
{
    auto tankConfig = _tanksConfig->getTankConfig(id);
    Q_CHECK_PTR(tankConfig);

    //сохраняем приход в БД НИТа
    if (tankConfig->status() != TankConfig::Status::REPAIR)
    {
        _db.transaction();
        QSqlQuery query(_db);

        for (const auto& intake: intakes)
        {
            const auto queryText =
                QString("INSERT INTO [%1].[dbo].[AddProduct] "
                        "([DateTime], [AZSCode], [TankNumber], [Product], [StartDateTime], [FinishedDateTime], [Height], [Volume], [Temp], [Density], [Mass]) "
                        "VALUES (CAST('%1' AS DATETIME2), '%2', %3, '%4', CAST('%5' AS DATETIME2), CAST('%6' AS DATETIME2), %7, %8, %9, %10, %11, %12)")
                    .arg(tankConfig->serviceDB())
                    .arg(QDateTime::currentDateTime().toString(DATETIME_FORMAT))
                    .arg(tankConfig->tankId().levelGaugeCode())
                    .arg(tankConfig->tankId().tankNumber())
                    .arg(tankConfig->product())
                    .arg(intake.startTankStatus().dateTime().addSecs(tankConfig->timeShift()).toString(DATETIME_FORMAT))
                    .arg(intake.finishTankStatus().dateTime().addSecs(tankConfig->timeShift()).toString(DATETIME_FORMAT))
                    .arg(intake.finishTankStatus().height() / 10.0, 0, 'f', 1)
                    .arg(intake.finishTankStatus().volume(), 0, 'f', 0)
                    .arg(intake.finishTankStatus().temp(), 0, 'f', 1)
                    .arg(intake.finishTankStatus().density(), 0, 'f', 1)
                    .arg(intake.finishTankStatus().mass(), 0, 'f', 0);

            if (!query.exec(queryText))
            {
                _db.rollback();

                emit errorOccurred(EXIT_CODE::LOAD_CONFIG_ERR, QString("Cannot save intake to Nit DB. Error: %1").arg(executeDBErrorString(_db, query)));

                return;
            }
        }

        if (!_db.commit())
        {
            _db.rollback();

            emit errorOccurred(EXIT_CODE::SQL_COMMIT_ERR, commitDBErrorString(_db));

            return;
        }
    }
}

void Sync::saveIntakes(const LevelGaugeService::TankID& id, const LevelGaugeService::IntakesList& intakes)
{
    auto tankConfig = _tanksConfig->getTankConfig(id);
    Q_CHECK_PTR(tankConfig);

    //вставляем в нашу таблицу
    QDateTime lastIntake;

    _db.transaction();
    QSqlQuery query(_db);

    for (const auto& intake: intakes)
    {
        const auto queryText =
            QString("INSERT INTO [dbo].[TanksIntake] "
                    "([DateTime], [AZSCode], [TankNumber], [Product], "
                    " [StartDateTime] ,[StartHeight], [StartVolume], [StartTemp], [StartDensity], [StartMass], "
                    " [FinishDateTime], [FinishHeight], [FinishStartVolume], [FinishtTemp], [FinishDensity], [FinishMass]) "
                    "VALUES (CAST('%1' AS DATETIME2), '%2', %3, '%4', "
                    " CAST('%5' AS DATETIME2), %6, %7, %8, %9, %10, %11, "
                    " CAST('%12' AS DATETIME2), %13, %14, %15, %16, %17, %18, "
                    " %19)")
                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
                .arg(tankConfig->tankId().levelGaugeCode())
                .arg(tankConfig->tankId().tankNumber())
                .arg(tankConfig->product())
                .arg(intake.startTankStatus().dateTime().addSecs(tankConfig->timeShift()).toString(DATETIME_FORMAT))
                .arg(intake.startTankStatus().height() / 10.0, 0, 'f', 1)
                .arg(intake.startTankStatus().volume(), 0, 'f', 0)
                .arg(intake.startTankStatus().temp(), 0, 'f', 1)
                .arg(intake.startTankStatus().density(), 0, 'f', 1)
                .arg(intake.startTankStatus().mass(), 0, 'f', 0)
                .arg(intake.finishTankStatus().dateTime().addSecs(tankConfig->timeShift()).toString(DATETIME_FORMAT))
                .arg(intake.finishTankStatus().height() / 10.0, 0, 'f', 1)
                .arg(intake.finishTankStatus().volume(), 0, 'f', 0)
                .arg(intake.finishTankStatus().temp(), 0, 'f', 1)
                .arg(intake.finishTankStatus().density(), 0, 'f', 1)
                .arg(intake.finishTankStatus().mass(), 0, 'f', 0);

        if (!query.exec(queryText))
        {
            _db.rollback();

            emit errorOccurred(EXIT_CODE::LOAD_CONFIG_ERR, QString("Cannot save intake to Nit DB. Error: %1").arg(executeDBErrorString(_db, query)));

            return;
        }

        lastIntake = std::max(lastIntake, intake.finishTankStatus().dateTime());
    }

    if (!_db.commit())
    {
        _db.rollback();

        emit errorOccurred(EXIT_CODE::SQL_COMMIT_ERR, commitDBErrorString(_db));

        return;
    }

    tankConfig->setLastIntake(lastIntake);
}
