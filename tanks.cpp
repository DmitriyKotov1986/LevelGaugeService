#include "tankstatus.h"

#include "tanks.h"

using namespace LevelGaugeService;
using namespace Common;

static const QString TANKS_CONNECTION_TO_DB_NAME = "TANKS_DB";

Tanks::Tanks(const Common::DBConnectionInfo dbConnectionInfo, LevelGaugeService::TanksConfig *tanksConfig, QObject *parent /* = nullptr */)
    : QObject{parent}
    , _dbConnectionInfo(dbConnectionInfo)
    , _tanksConfig(tanksConfig)
{
    Q_CHECK_PTR(_tanksConfig);

    qRegisterMetaType<LevelGaugeService::TankStatusesList>("TankStatusesList");
    qRegisterMetaType<LevelGaugeService::IntakesList>("IntakesList");
}

Tanks::~Tanks()
{
    stop();
}

void Tanks::start()
{
    Q_ASSERT(!_isStarted);
    Q_ASSERT(_checkNewMeasumentsTimer == nullptr);
    Q_ASSERT(!_db.isOpen());

    //подключаемся к БД
    try
    {
        connectToDB(_db, _dbConnectionInfo, TANKS_CONNECTION_TO_DB_NAME);
    }
    catch (const SQLException& err)
    {
        emit errorOccurred(EXIT_CODE::SQL_NOT_CONNECT, err.what());

        return;
    }

    _checkNewMeasumentsTimer = new QTimer();

    QObject::connect(_checkNewMeasumentsTimer, SIGNAL(timeout()), SLOT(loadFromMeasumentsDB()));

    makeTanks();

    _isStarted = true;
}

void Tanks::stop()
{
    if (!_isStarted)
    {
        return;
    }

    //checkNewMeasumentsTimer
    delete _checkNewMeasumentsTimer;

    _checkNewMeasumentsTimer = nullptr;

    //Tanks
    emit stopAll();

    for (auto& tank: _tanks)
    {
        tank.second->thread->wait();
    }
    _tanks.clear();

    emit finished();
}

void Tanks::calculateStatusesTank(const TankID &id, const TankStatusesList &tankStatuses)
{
    Q_ASSERT(_isStarted);

    emit calculateStatuses(id, tankStatuses);
}

void Tanks::calculateIntakesTank(const TankID &id, const IntakesList &intakes)
{
    Q_ASSERT(_isStarted);

    emit calculateIntakes(id, intakes);
}

void Tanks::errorOccurredTank(const LevelGaugeService::TankID& id, Common::EXIT_CODE errorCode, const QString& msg)
{    
    emit errorOccurred(errorCode, QString("Tank error. AZSCode: %1. TankNumber: %2. Error: %3")
                       .arg(id.levelGaugeCode())
                       .arg(id.tankNumber())
                       .arg(msg));
}

void Tanks::sendLogMsgTank(const TankID &id, Common::TDBLoger::MSG_CODE category, const QString &msg)
{
    emit sendLogMsg(category, QString("Tank: AZSCode: %1. TankNumber: %2. Message: %3")
                        .arg(id.levelGaugeCode())
                        .arg(id.tankNumber())
                        .arg(msg));
}

void Tanks::startedTank(const TankID &id)
{
    Q_ASSERT(_tanks.contains(id));
    Q_CHECK_PTR(_checkNewMeasumentsTimer);

    _tanks.at(id)->isStarted = true;

    const auto allStarted = std::all_of(_tanks.begin(), _tanks.end(),
        [](const auto& tank)
        {
            return tank.second->isStarted;
        }
    );

    if (allStarted)
    {
        _checkNewMeasumentsTimer->start(60000);
    }
}

QString Tanks::tanksFilterCalculate() const
{
    bool isFirst = true;
    QString result;
    for (const auto& tankId: _tanksConfig->getTanksID())
    {
        if (!isFirst)
        {
            result += " OR ";
        }
        isFirst = false;

        const auto tankConfig = _tanksConfig->getTankConfig(tankId);

        const auto lastSave = std::min(tankConfig->lastSave(), tankConfig->lastIntake());

        result += QString("([AZSCode] = '%1' AND [TankNumber] = %2 AND [DateTime] > CAST('%3' AS DATETIME2))")
                .arg(tankId.levelGaugeCode())
                .arg(tankId.tankNumber())
                .arg(lastSave.toString(DATETIME_FORMAT));
    }

    return result;
}

Tanks::TanksLoadStatuses Tanks::loadFromCalculatedDB()
{
    Q_ASSERT(!_isStarted);
    Q_ASSERT(_db.isOpen());

    //т.к. приоритетное значение имеет сохранненные измерения - то сначала загружаем их
    const auto queryText =
        QString("SELECT [ID], [AZSCode], [TankNumber], [DateTime], [Volume], [Mass], [Density], [Height], [Temp], [AdditionFlag], [Status] "
                "FROM [TanksCalculate] "
                "WHERE (%1) "
                "ORDER BY [DateTime] DESC ")
            .arg(tanksFilterCalculate());

    TanksLoadStatuses result;
    quint64 countStatuses = 0;
    std::unordered_map<TankID, QDateTime> lastMeasuments;

    try
    {
        transactionDB(_db);

        QSqlQuery query(_db);
        query.setForwardOnly(true);

        Common::DBQueryExecute(_db, query, queryText);

        while (query.next())
        {
            class TankStatusLoadException
                : public std::runtime_error
            {
            public:
                explicit TankStatusLoadException(const QString& what)
                    : std::runtime_error(what.toStdString())
                {}
            };

            try
            {
                const auto recordID = query.value("ID").toULongLong();
                const auto AZSCode = query.value("AZSCode").toString();
                if (AZSCode.isEmpty())
                {
                    throw TankStatusLoadException(QString("Value [TanksCalculate]/AZSCode cannot be empty. Record ID: %1").arg(recordID));
                }

                const auto tankNumber = query.value("TankNumber").toUInt();
                if (tankNumber == 0)
                {
                    throw TankStatusLoadException(QString("Value [TanksCalculate]/TankNumber cannot be empty. Record ID: %1").arg(recordID));
                }
                const auto id = TankID(AZSCode, tankNumber);

                if (!_tanksConfig->checkID(id))
                {
                    continue;
                    //throw TankStatusLoadException(QString("Tank with ID %1 have not config on [TanksInfo]. Record ID: %2").arg(id.toString()).arg(recordID));
                }
                auto tankConfig = _tanksConfig->getTankConfig(id);

                TankStatus::TankStatusData tmp;
                tmp.dateTime = query.value("DateTime").toDateTime();
                if (tankConfig->lastIntake() > tmp.dateTime.addDays(-1))
                {
                    continue;
                }

                tmp.density = query.value("Density").toFloat();
                tmp.height = query.value("Height").toFloat(); //высота в мм
                tmp.mass = query.value("Mass").toFloat();
                tmp.temp = query.value("Temp").toFloat();
                tmp.volume = query.value("Volume").toFloat();
                tmp.additionFlag = query.value("AdditionFlag").toUInt();
                tmp.status = TankConfig::intToStatus(query.value("Status").toUInt());

                if (!tmp.check())
                {
                    throw TankStatusLoadException(QString("Invalid value tank status from DB [TanksCalculate]. Record ID: %1").arg(recordID));
                }

                auto lastMeasuments_it = lastMeasuments.find(id);
                if (lastMeasuments_it == lastMeasuments.end())
                {
                    lastMeasuments.emplace(id, tmp.dateTime);
                }
                else
                {
                    lastMeasuments_it->second = std::max(tmp.dateTime, lastMeasuments_it->second);
                }

                TankStatus tankStatus(std::move(tmp));
                auto& tankStatusList = result[id];
                tankStatusList.emplace_back(std::move(tankStatus));
            }
            catch (TankStatusLoadException& err)
            {
                emit sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("Cannot load tank status from DB [TanksCalculate]. Tank skipped. Error: %1").arg(err.what()));
            }

            ++countStatuses;
        }

        commitDB(_db);
    }
    catch (const SQLException& err)
    {
        _db.rollback();

        emit errorOccurred(EXIT_CODE::SQL_EXECUTE_QUERY_ERR, err.what());

        return result;
    }

    for (const auto& lastMeasument: lastMeasuments)
    {
        auto tankConfig = _tanksConfig->getTankConfig(lastMeasument.first);
        if (lastMeasument.second > tankConfig->lastMeasuments())
        {
            tankConfig->setLastMeasuments(lastMeasument.second);
        }
    }

    emit sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Load saved statuses from DB [TanksCalculate] complited. Count saved statuses: %1").arg(countStatuses));

    return result;
}

QString Tanks::tanksFilterMeasument() const
{
    bool isFirst = true;
    QString result;
    for (const auto& tankId: _tanksConfig->getTanksID())
    {
        if (!isFirst)
        {
            result += " OR ";
        }
        isFirst = false;

        const auto tankConfig = _tanksConfig->getTankConfig(tankId);

        result += QString("([AZSCode] = '%1' AND [TankNumber] = %2 AND [DateTime] > CAST('%3' AS DATETIME2))")
                .arg(tankId.levelGaugeCode())
                .arg(tankId.tankNumber())
                .arg(tankConfig->lastMeasuments().toString(DATETIME_FORMAT));
    }

    return result;
}


void Tanks::loadFromMeasumentsDB()
{
    Q_ASSERT(_db.isOpen());

    QString queryText;
    if (_lastLoadId == 0)
    {
        QDateTime lastMeasument = QDateTime::currentDateTime().addYears(1);
        for (const auto& tankId: _tanksConfig->getTanksID())
        {
            const auto tankConfig = _tanksConfig->getTankConfig(tankId);

            lastMeasument = std::min(lastMeasument, tankConfig->lastMeasuments());
        }

        queryText =
            QString("SELECT [ID], [AZSCode], [TankNumber], [DateTime], [Density], [Height], [Volume], [Mass], [Temp] "
                    "FROM [TanksMeasument] "
                    "WHERE (%1) ")
                .arg(tanksFilterMeasument());

    }
    else
    {
        queryText =
            QString("SELECT [ID], [AZSCode], [TankNumber], [DateTime], [Density], [Height], [Volume], [Mass], [Temp] "
                    "FROM [TanksMeasument] "
                    "WHERE [ID] > %1 ")
            .arg(_lastLoadId);
    }
    Q_ASSERT(!queryText.isEmpty());

    TanksLoadStatuses tanksStatuses;
    quint64 countNewStatuses = 0;
    std::unordered_map<TankID, QDateTime> lastMeasuments;

    try
    {
        transactionDB(_db);

        QSqlQuery query(_db);
        query.setForwardOnly(true);

        DBQueryExecute(_db, query, queryText);

        auto lastDateTime = QDateTime::currentDateTime();
        while (query.next())
        {
            const auto recordID = query.value("ID").toULongLong();

            class TankStatusLoadException
                : public std::runtime_error
            {
            public:
                explicit TankStatusLoadException(const QString& what)
                    : std::runtime_error(what.toStdString())
                {}
            };

            try
            {
                const auto AZSCode = query.value("AZSCode").toString();
                if (AZSCode.isEmpty())
                {
                    throw TankStatusLoadException(QString("Value [TanksMeasument]/AZSCode cannot be empty. Record ID: %1").arg(recordID));
                }
    
                const auto tankNumber = query.value("TankNumber").toUInt();
                if (tankNumber == 0)
                {
                    throw TankStatusLoadException(QString("Value [TanksMeasument]/TankNumber cannot be empty. Record ID: %1").arg(recordID));
                }
                const auto id = TankID(AZSCode, tankNumber);
    
                if (!_tanksConfig->checkID(id))
                {
                    continue;
                 //   throw TankStatusLoadException(QString("Tank with ID %1 have not config on [TanksInfo]. Record ID: %2").arg(id.toString()).arg(recordID));
                }
    
                auto tankConfig = _tanksConfig->getTankConfig(id);
    
                TankStatus::TankStatusData tmp;
                tmp.dateTime = query.value("DateTime").toDateTime();
                if (tankConfig->lastMeasuments() > tmp.dateTime)
                {
                    continue;
                }
    
                tmp.density = query.value("Density").toFloat();
                tmp.height = query.value("Height").toFloat();
                tmp.mass = query.value("Mass").toFloat();
                tmp.temp = query.value("Temp").toFloat();
                tmp.volume = query.value("Volume").toFloat();
                tmp.additionFlag = static_cast<quint8>(TankStatus::AdditionFlag::MEASUMENTS);
                if (tankConfig->status() == TankConfig::Status::REPAIR)
                {
                    tmp.status = TankConfig::Status::REPAIR;
                }
                else
                {
                    tmp.status = tankConfig->status() != TankConfig::Status::UNDEFINE ? tankConfig->status() : TankConfig::Status::STUGLE;
                }
    
                if (!tmp.check())
                {
                    throw TankStatusLoadException(QString("Invalid value tank status from [TanksMeasument]. Record ID: %1").arg(recordID));
                }
    
                auto lastMeasuments_it = lastMeasuments.find(id);
                if (lastMeasuments_it == lastMeasuments.end())
                {
                    lastMeasuments.emplace(id, tmp.dateTime);
                }
                else
                {
                    lastMeasuments_it->second = std::max(tmp.dateTime, lastMeasuments_it->second);
                }
    
                TankStatus tankStatus(std::move(tmp));
                auto& tankStatusList = tanksStatuses[id];
                tankStatusList.emplace_back(std::move(tankStatus));
            }
            catch (TankStatusLoadException& err)
            {
                emit sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("Cannot load tank status from DB. Tank skipped. Error: %1").arg(err.what()));
            }
    
            _lastLoadId = std::max(_lastLoadId, recordID);
            ++countNewStatuses;
        }

        commitDB(_db);
    }
    catch (const SQLException& err)
    {
        _db.rollback();

        emit errorOccurred(EXIT_CODE::LOAD_CONFIG_ERR, QString("Cannot load tanks statuses from [TanksMeasument]. Error: %1").arg(err.what()));

        return;
    }

    for (const auto& lastMeasument: lastMeasuments)
    {
        auto tankConfig = _tanksConfig->getTankConfig(lastMeasument.first);
        if (lastMeasument.second > tankConfig->lastMeasuments())
        {
            tankConfig->setLastMeasuments(lastMeasument.second);
        }
    }

    emit sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Load new statuses from DB [TanksMeasument] complited. Count new statuses: %1").arg(countNewStatuses));

    for (auto tanksStatuses_it = tanksStatuses.begin(); tanksStatuses_it != tanksStatuses.end(); ++tanksStatuses_it)
    {
        emit newStatuses(tanksStatuses_it->first, tanksStatuses_it->second);
    }
}

void Tanks::makeTanks()
{
    Q_CHECK_PTR(_tanksConfig);

    const auto tanksSavedStatuses = loadFromCalculatedDB();

    for (const auto& tankId: _tanksConfig->getTanksID())
    {
        auto tmp = std::make_unique<TankThread>();

        auto tankConfig = _tanksConfig->getTankConfig(tankId);

        auto tanksSavedStatuses_it = tanksSavedStatuses.find(tankId);
        if (tanksSavedStatuses_it != tanksSavedStatuses.end())
        {
            tmp->tank = std::make_unique<Tank>(tankConfig, tanksSavedStatuses_it->second);
        }
        else
        {
            tmp->tank = std::make_unique<Tank>(tankConfig, TankStatusesList{});
        }
        tmp->thread = std::make_unique<QThread>();

        tmp->tank->moveToThread(tmp->thread.get());

        QObject::connect(tmp->thread.get(), SIGNAL(started()), tmp->tank.get(), SLOT(start()), Qt::QueuedConnection);
        QObject::connect(tmp->tank.get(), SIGNAL(finished()), tmp->thread.get(), SLOT(quit()), Qt::DirectConnection);

        QObject::connect(this, SIGNAL(stopAll()), tmp->tank.get(), SLOT(stop()), Qt::QueuedConnection);

        QObject::connect(tmp->tank.get(), SIGNAL(errorOccurred(const LevelGaugeService::TankID&, Common::EXIT_CODE, const QString&)),
                         SLOT(errorOccurredTank(const LevelGaugeService::TankID&, Common::EXIT_CODE, const QString&)), Qt::QueuedConnection);
        QObject::connect(tmp->tank.get(), SIGNAL(sendLogMsg(const LevelGaugeService::TankID&, Common::TDBLoger::MSG_CODE, const QString&)),
                         SLOT(sendLogMsgTank(const LevelGaugeService::TankID&, Common::TDBLoger::MSG_CODE, const QString &)), Qt::QueuedConnection);

        QObject::connect(this, SIGNAL(newStatuses(const LevelGaugeService::TankID&, const LevelGaugeService::TankStatusesList&)),
                         tmp->tank.get(), SLOT(newStatuses(const LevelGaugeService::TankID&, const LevelGaugeService::TankStatusesList&)), Qt::QueuedConnection);

        QObject::connect(tmp->tank.get(), SIGNAL(calculateStatuses(const LevelGaugeService::TankID&, const TankStatusesList&)),
                         SLOT(calculateStatusesTank(const LevelGaugeService::TankID&, const TankStatusesList&)), Qt::QueuedConnection);
        QObject::connect(tmp->tank.get(), SIGNAL(calculateIntakes(const LevelGaugeService::TankID&, const IntakesList&)),
                         SLOT(calculateIntakesTank(const LevelGaugeService::TankID&, const IntakesList&)), Qt::QueuedConnection);

        QObject::connect(tmp->tank.get(), SIGNAL(started(const LevelGaugeService::TankID&)),
                        SLOT(startedTank(const LevelGaugeService::TankID&)), Qt::QueuedConnection);

        _tanks.emplace(tankId, std::move(tmp));
    }

    //запускаем резервуары с лагом по времени чтобы сбалансировать нагрузку
    quint64 tankNumber = 0;
    for (const auto& tankId: _tanksConfig->getTanksID())
    {
        auto& tankThread =  _tanks.at(tankId)->thread;
        QTimer::singleShot((60000 / _tanks.size()) * tankNumber, this, [&tankThread](){ tankThread->start(); });
    }

    //далее ждем когда все емкости запустяться и придут сигналы Tank::started(...)
}
