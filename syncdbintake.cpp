//My
#include "Common/common.h"

#include "syncdbintake.h"

using namespace LevelGaugeService;
using namespace Common;

static const QString CONNECTION_TO_DB_NAME = "SyncDBIntake";
static const QString SYNC_NAME = "SyncDBIntake";

SyncDBIntake::SyncDBIntake(const Common::DBConnectionInfo& dbConnectionInfo,
           LevelGaugeService::TanksConfig* tanksConfig,
           QObject *parent /* = nullptr */)
    : SyncImpl{parent}
    , _tanksConfig(tanksConfig)
    , _dbConnectionInfo(dbConnectionInfo)
{
    Q_CHECK_PTR(_tanksConfig);
}

SyncDBIntake::~SyncDBIntake()
{
    stop();
}

void SyncDBIntake::start()
{
    Q_ASSERT(!_isStarted);

    try
    {
        connectToDB(_db, _dbConnectionInfo, QString("%1").arg(CONNECTION_TO_DB_NAME));
    }
    catch (const SQLException& err)
    {
        emit errorOccurred(SYNC_NAME, EXIT_CODE::SQL_NOT_CONNECT, err.what());

        return;
    }

    _isStarted = true;
}

void SyncDBIntake::stop()
{
    if (!_isStarted)
    {
        return;
    }

    closeDB(_db);
}


void SyncDBIntake::calculateIntakes(const LevelGaugeService::TankID& id, const IntakesList &intakes)
{
    Q_ASSERT(!intakes.empty());
    Q_ASSERT(_isStarted);

    saveIntakesToDB(id, intakes);

    QDateTime lastIntake = QDateTime::currentDateTime().addYears(-100);
    for (const auto& intake: intakes)
    {
        lastIntake = std::max(lastIntake, intake.finishTankStatus().dateTime());
    }

    auto tankConfig = _tanksConfig->getTankConfig(id);
    tankConfig->setLastIntake(lastIntake);
}

void SyncDBIntake::saveIntakesToDB(const LevelGaugeService::TankID& id, const LevelGaugeService::IntakesList& intakes)
{
    Q_ASSERT(!intakes.empty());
    Q_ASSERT(_db.isOpen());

    const auto tankConfig = _tanksConfig->getTankConfig(id);

    //вставляем в нашу таблицу
    QDateTime lastIntake = QDateTime::currentDateTime().addYears(-100);
    for (const auto& intake: intakes)
    {
        const auto queryText =
            QString("INSERT INTO [dbo].[TanksIntake] "
                        "([DateTime], [AZSCode], [TankNumber], [Product], [Status], "
                        "[StartDateTime] ,[StartHeight], [StartVolume], [StartTemp], [StartDensity], [StartMass], "
                        "[FinishDateTime], [FinishHeight], [FinishVolume], [FinishTemp], [FinishDensity], [FinishMass]) "
                    "VALUES (CAST('%1' AS DATETIME2), '%2', %3, '%4', %5, "
                        "CAST('%6' AS DATETIME2), %7, %8, %9, %10, %11, "
                        "CAST('%12' AS DATETIME2), %13, %14, %15, %16, %17)")
                .arg(QDateTime::currentDateTime().toString(DATETIME_FORMAT))
                .arg(id.levelGaugeCode())
                .arg(id.tankNumber())
                .arg(tankConfig->product())
                .arg(static_cast<quint8>(tankConfig->status()))
                .arg(intake.startTankStatus().dateTime().addSecs(tankConfig->timeShift()).toString(DATETIME_FORMAT))
                .arg(intake.startTankStatus().height(), 0, 'f', 1)
                .arg(intake.startTankStatus().volume(), 0, 'f', 0)
                .arg(intake.startTankStatus().temp(), 0, 'f', 1)
                .arg(intake.startTankStatus().density(), 0, 'f', 1)
                .arg(intake.startTankStatus().mass(), 0, 'f', 0)
                .arg(intake.finishTankStatus().dateTime().addSecs(tankConfig->timeShift()).toString(DATETIME_FORMAT))
                .arg(intake.finishTankStatus().height(), 0, 'f', 1)
                .arg(intake.finishTankStatus().volume(), 0, 'f', 0)
                .arg(intake.finishTankStatus().temp(), 0, 'f', 1)
                .arg(intake.finishTankStatus().density(), 0, 'f', 1)
                .arg(intake.finishTankStatus().mass(), 0, 'f', 0);

        try
        {
            DBQueryExecute(_db, queryText);
        }
        catch (const SQLException& err)
        {
            emit errorOccurred(SYNC_NAME, EXIT_CODE::SQL_EXECUTE_QUERY_ERR, err.what());

            return;
        }
    }
}
