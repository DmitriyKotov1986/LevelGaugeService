//My
#include "Common/common.h"

#include "syncdbstatus.h"

using namespace LevelGaugeService;
using namespace Common;

static const QString CONNECTION_TO_DB_NAME = "SyncDBStatus";
static const QString SYNC_NAME = "SyncToDBStatus";

SyncDBStatus::SyncDBStatus(const Common::DBConnectionInfo& dbConnectionInfo,
           LevelGaugeService::TanksConfig* tanksConfig,
           QObject *parent /* = nullptr */)
    : SyncImpl{parent}
    , _tanksConfig(tanksConfig)
    , _dbConnectionInfo(dbConnectionInfo)
{
    Q_CHECK_PTR(_tanksConfig);
}

SyncDBStatus::~SyncDBStatus()
{
    stop();
}

void SyncDBStatus::start()
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

    _saveTimer = new QTimer();

    QObject::connect(_saveTimer, SIGNAL(timeout()), SLOT(saveToDB()));

    _saveTimer->start(30000);

    _isStarted = true;
}

void SyncDBStatus::stop()
{
    if (!_isStarted)
    {
        return;
    }

    saveToDB();

    delete _saveTimer;

    closeDB(_db);
}

void SyncDBStatus::calculateStatuses(const LevelGaugeService::TankID& id, const LevelGaugeService::TankStatusesList &tankStatuses)
{
    Q_ASSERT(!tankStatuses.empty());
    Q_ASSERT(_isStarted);

    auto dataForSave_it = _dataForSave.find(id);
    if (dataForSave_it == _dataForSave.end())
    {
         dataForSave_it = _dataForSave.insert(id, LevelGaugeService::TankStatusesList{});
    }

    for(const auto& status: tankStatuses)
    {
        dataForSave_it.value().push_back(status);
    }
}

void SyncDBStatus::saveToDB()
{
    Q_ASSERT(_db.isOpen());

    if ( _dataForSave.empty())
    {
        emit sendLogMsg(SYNC_NAME, TDBLoger::MSG_CODE::INFORMATION_CODE, "No statuses for save to DB");

        return;
    }

    QHash<LevelGaugeService::TankID, QDateTime> lastStatuses;

    quint64 recordCount = 0;

    try
    {
        transactionDB(_db);

        QSqlQuery query(_db);

        for (auto dataForSave_it = _dataForSave.begin(); dataForSave_it != _dataForSave.end(); ++dataForSave_it)
        {
            const auto tankConfig = _tanksConfig->getTankConfig(dataForSave_it.key());
            const auto& tankId = tankConfig->tankId();

            auto lastStatus_it = lastStatuses.insert(tankId, QDateTime::currentDateTime().addYears(-100));

            for (auto data_it = dataForSave_it.value().begin(); data_it != dataForSave_it.value().end(); ++data_it)
            {
                const auto queryText =
                    QString("INSERT INTO [dbo].[TanksCalculate] "
                            "([AZSCode], [TankNumber], [DateTime], "
                            "[Volume], [TotalVolume], [Mass], [Density], [Height], [Temp], [Product], [ProductStatus], "
                            "[TankName], [Type], [AdditionFlag], [Status], [Mode], [SaveDateTime]) "
                        "VALUES ("
                            "'%1', %2, CAST('%3' AS DATETIME2), "
                            "%4, %5, %6, %7, %8, %9, '%10', %11, "
                            "'%12', %13, %14, %15, %16, CAST('%17' AS DATETIME2))")
                .arg(tankId.levelGaugeCode())                                                           //1
                .arg(tankId.tankNumber())                                                               //2
                .arg(data_it->dateTime().addSecs(tankConfig->timeShift()).toString(DATETIME_FORMAT))    //3
                .arg(data_it->volume(), 0, 'f', 0)                                                      //4
                .arg(tankConfig->totalVolume(), 0, 'f', 0)                                              //5
                .arg(data_it->mass(), 0, 'f', 0)                                                        //6
                .arg(data_it->density(), 0, 'f', 1)                                                     //7
                .arg(data_it->height(), 0, 'f', 1)                                                      //8
                .arg(data_it->temp(), 0, 'f', 1)                                                        //9
                .arg(tankConfig->product())                                                             //10
                .arg(static_cast<quint8>(tankConfig->productStatus()))                                  //11
                .arg(tankConfig->name())                                                                //12
                .arg(static_cast<quint8>(tankConfig->type()))                                           //13
                .arg(static_cast<quint8>(data_it->additionFlag()))                                      //14
                .arg(static_cast<quint8>(data_it->status()))                                            //15
                .arg( static_cast<quint8>(tankConfig->mode()))                                          //16
                .arg((QDateTime::currentDateTime().toString(DATETIME_FORMAT)));                         //17

                *lastStatus_it = std::max(lastStatus_it.value(), data_it->dateTime());

                if (!query.exec(queryText))
                {
                    throw SQLException(executeDBErrorString(_db, query));
                }

                ++recordCount;
            }
        }

        commitDB(_db);

        _dataForSave.clear();
    }
    catch (const SQLException& err)
    {
        _db.rollback();

        emit errorOccurred(SYNC_NAME, EXIT_CODE::SQL_EXECUTE_QUERY_ERR, err.what());
    }

    QString lastStatusStr;
    bool isFirst = true;

    for (auto lastStatuses_it = lastStatuses.begin(); lastStatuses_it != lastStatuses.end(); ++lastStatuses_it)
    {
        auto tankConfig = _tanksConfig->getTankConfig(lastStatuses_it.key());
        tankConfig->setLastSave(lastStatuses_it.value());

        if (!isFirst)
        {
            lastStatusStr += ", ";
        }
        isFirst = false;

        lastStatusStr += QString("%1=%2").arg(lastStatuses_it.key().toString()).arg(lastStatuses_it.value().toString(DATETIME_FORMAT));
    }

    emit sendLogMsg(SYNC_NAME, TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Statuses saved to DB successfull. Count: %1. New last save time: %2").arg(recordCount).arg(lastStatusStr));
}
