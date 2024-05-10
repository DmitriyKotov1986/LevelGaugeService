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
    Q_ASSERT(_checkNewMeasumentsTimer == nullptr);

    Q_ASSERT(!_db.isOpen());

    //подключаемся к БД
    if (!connectToDB(_db, _dbConnectionInfo, TANKS_CONNECTION_TO_DB_NAME))
    {
        emit errorOccurred(EXIT_CODE::SQL_NOT_CONNECT, connectDBErrorString(_db));

        return;
    };

    if (!makeTanks())
    {
        return;
    }

    _checkNewMeasumentsTimer = new QTimer();

    QObject::connect(_checkNewMeasumentsTimer, SIGNAL(timeout()), SLOT(checkNewMeasuments()));

    _checkNewMeasumentsTimer->start(60000);
}

void Tanks::stop()
{
    if (!_checkNewMeasumentsTimer)
    {
        return;
    }

    //checkNewMeasumentsTimer
    _checkNewMeasumentsTimer->stop();

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

void Tanks::calculateStatusesTank(const TankID &id, const TankStatusesList& tankStatuses)
{
    Q_ASSERT(!tankStatuses.isEmpty());

    emit calculateStatuses(id, tankStatuses);
}

void Tanks::calculateIntakesTank(const TankID &id, const IntakesList &intakes)
{
    Q_ASSERT(!intakes.empty());

    emit calculateIntakes(id, intakes);
}

void Tanks::errorOccurredTank(const LevelGaugeService::TankID& id, Common::EXIT_CODE errorCode, const QString& msg)
{
    emit errorOccurred(errorCode, QString("Tank error. AZSCode: %1 TankNumber: %2 Error: %3")
                       .arg(id.levelGaugeCode())
                       .arg(id.tankNumber())
                       .arg(msg));
}

void Tanks::sendLogMsgTank(const TankID &id, Common::TDBLoger::MSG_CODE category, const QString &msg)
{
    emit sendLogMsg(category, QString("Tank: AZSCode: %1 TankNumber: %2 Message: %3")
                        .arg(id.levelGaugeCode())
                        .arg(id.tankNumber())
                        .arg(msg));
}

void Tanks::checkNewMeasuments()
{
    loadFromMeasumentsDB();
}

bool Tanks::loadFromCalculatedDB()
{
    Q_ASSERT(_db.isOpen());

    QDateTime lastSave = QDateTime::currentDateTime().addYears(1);
    for (const auto& tankId: _tanksConfig->getTanksID())
    {
        const auto tankConfig_p = _tanksConfig->getTankConfig(tankId);
        Q_CHECK_PTR(tankConfig_p);

        lastSave = std::min(lastSave, tankConfig_p->lastSave());
        lastSave = std::min(lastSave, tankConfig_p->lastIntake());
    }
    lastSave = lastSave.addDays(-1);

    //т.к. приоритетное значение имеет сохранненные измерения - то сначала загружаем их
    _db.transaction();
    QSqlQuery query(_db);
    query.setForwardOnly(true);

    const auto queryText =
        QString("SELECT"
                    "[ID], [AZSCode], [TankNumber], [DateTime], [Volume], [Mass], [Density], [Height], [Temp], [AdditionFlag], [Status] "
                "FROM [TanksCalculate] "
                "WHERE "
                    "[DateTime] > CAST('%1' AS DATETIME2) "
                "ORDER BY [DateTime] DESC ")
            .arg(lastSave.toString(DATETIME_FORMAT));


    if (!query.exec(queryText))
    {
        _db.rollback();

        emit errorOccurred(EXIT_CODE::LOAD_CONFIG_ERR, QString("Cannot load tanks statuses from [TanksCalculate]. Error: %1").arg(executeDBErrorString(_db, query)));

        return false;
    }

    class TankStatusLoadException
        : public std::runtime_error
    {
    public:
        explicit TankStatusLoadException(const QString& what)
            : std::runtime_error(what.toStdString())
        {}
    };

    QHash<TankID, TankStatusesList> tanksStatuses;

    //сохраняем
    auto lastDateTime = QDateTime::currentDateTime();
    while (query.next())
    {
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

            auto tankConfig_p = _tanksConfig->getTankConfig(id);
            if (!tankConfig_p)
            {
                continue;
            }

            TankStatus::TankStatusData tmp;
            tmp.dateTime = query.value("DateTime").toDateTime();
            tmp.density = query.value("Density").toFloat();
            tmp.height = query.value("Height").toFloat() * 10; //переводим высоту обратно в мм
            tmp.mass = query.value("Mass").toFloat();
            tmp.temp = query.value("Temp").toFloat();
            tmp.volume = query.value("Volume").toFloat();
            tmp.additionFlag = query.value("AdditionFlag").toUInt();
            tmp.status = TankConfig::intToStatus(query.value("Status").toUInt());

            if (!tmp.check())
            {
                throw TankStatusLoadException(QString("Invalid value tank status from [TanksCalculate]. Record ID: %1").arg(recordID));
            }

            if (tmp.dateTime > tankConfig_p->lastMeasuments())
            {
                _tanksConfig->getTankConfig(id)->setLastMeasuments(tmp.dateTime);
            }

            TankStatus tankStatus(std::move(tmp));
            tanksStatuses[id].emplaceBack(std::move(tankStatus));
        }
        catch (TankStatusLoadException& err)
        {
            emit sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("Cannot load tank status from DB. Tank skipped. Error: %1").arg(err.what()));
        }
    }

    if (!_db.commit())
    {
        _db.rollback();

        emit errorOccurred(EXIT_CODE::SQL_COMMIT_ERR, commitDBErrorString(_db));

        return false;
    }

    for (auto tanksStatuses_it = tanksStatuses.begin(); tanksStatuses_it != tanksStatuses.end(); ++tanksStatuses_it)
    {
        emit newStatuses(tanksStatuses_it.key(), tanksStatuses_it.value());
    }

    return true;
}

bool Tanks::loadFromMeasumentsDB()
{

    QString queryText;
    if (_lastLoadId == 0)
    {
        QDateTime lastMeasument = QDateTime::currentDateTime().addYears(1);
        for (const auto& tankId: _tanksConfig->getTanksID())
        {
            const auto tankConfig_p = _tanksConfig->getTankConfig(tankId);
            Q_CHECK_PTR(tankConfig_p);

            lastMeasument = std::min(lastMeasument, tankConfig_p->lastMeasuments());
        }

        queryText =
            QString("SELECT [ID], [AZSCode], [TankNumber], [DateTime], [Density], [Height], [Volume], [Mass], [Temp] "
                    "FROM [TanksMeasument] "
                    "WHERE [DateTime] > CAST('%1' AS DATETIME2) ")
                .arg(lastMeasument.toString(DATETIME_FORMAT));

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

    _db.transaction();
    QSqlQuery query(_db);
    query.setForwardOnly(true);

    if (!query.exec(queryText))
    {
        _db.rollback();

        emit errorOccurred(EXIT_CODE::LOAD_CONFIG_ERR, QString("Cannot load tanks statuses from [TanksMeasument]. Error: %1").arg(executeDBErrorString(_db, query)));

        return false;
    }

    class TankStatusLoadException
        : public std::runtime_error
    {
    public:
        explicit TankStatusLoadException(const QString& what)
            : std::runtime_error(what.toStdString())
        {}
    };

    QHash<TankID, TankStatusesList> tanksStatuses;

    //сохраняем
    auto lastDateTime = QDateTime::currentDateTime();
    while (query.next())
    {
        const auto recordID = query.value("ID").toULongLong();

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

            auto tankConfig_p = _tanksConfig->getTankConfig(id);
            if (!tankConfig_p)
            {
                continue;
            }

            TankStatus::TankStatusData tmp;
            tmp.dateTime = query.value("DateTime").toDateTime();
            tmp.density = query.value("Density").toFloat();
            tmp.height = query.value("Height").toFloat() * 10; //переводим высоту обратно в мм
            tmp.mass = query.value("Mass").toFloat();
            tmp.temp = query.value("Temp").toFloat();
            tmp.volume = query.value("Volume").toFloat();
            tmp.additionFlag = static_cast<quint8>(TankStatus::AdditionFlag::MEASUMENTS);
            if (tankConfig_p->status() == TankConfig::Status::REPAIR)
            tmp.status = tankConfig_p->status() != TankConfig::Status::UNDEFINE ? tankConfig_p->status() : TankConfig::Status::STUGLE;

            if (!tmp.check())
            {
                throw TankStatusLoadException(QString("Invalid value tank status from [TanksMeasument]. Record ID: %1").arg(recordID));
            }

            if (tmp.dateTime > tankConfig_p->lastMeasuments())
            {
                _tanksConfig->getTankConfig(id)->setLastMeasuments(tmp.dateTime);
            }

            TankStatus tankStatus(std::move(tmp));
            tanksStatuses[id].emplaceBack(std::move(tankStatus));
        }
        catch (TankStatusLoadException& err)
        {
            emit sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("Cannot load tank status from DB. Tank skipped. Error: %1").arg(err.what()));
        }

        _lastLoadId = std::max(_lastLoadId, recordID);
    }

    if (!_db.commit())
    {
        _db.rollback();

        emit errorOccurred(EXIT_CODE::LOAD_CONFIG_ERR, commitDBErrorString(_db));

        return false;
    }

    for (auto tanksStatuses_it = tanksStatuses.begin(); tanksStatuses_it != tanksStatuses.end(); ++tanksStatuses_it)
    {
        emit newStatuses(tanksStatuses_it.key(), tanksStatuses_it.value());
    }

    return true;
}

bool Tanks::makeTanks()
{
    Q_CHECK_PTR(_tanksConfig);

    for (const auto& tankId: _tanksConfig->getTanksID())
    {
        auto tmp = std::make_unique<TankThread>();

        const auto tankConfig_p = _tanksConfig->getTankConfig(tankId);
        Q_CHECK_PTR(tankConfig_p);

        tmp->tank = std::make_unique<Tank>(tankConfig_p);
        tmp->thread = std::make_unique<QThread>();

        tmp->tank->moveToThread(tmp->thread.get());

        QObject::connect(tmp->thread.get(), SIGNAL(started()), tmp->tank.get(), SLOT(start()), Qt::DirectConnection);
        QObject::connect(tmp->tank.get(), SIGNAL(finished()), tmp->thread.get(), SLOT(quit()), Qt::DirectConnection);

        QObject::connect(this, SIGNAL(stopAll()), tmp->tank.get(), SLOT(stop()), Qt::QueuedConnection);

        QObject::connect(tmp->tank.get(), SIGNAL(errorOccurred(const LevelGaugeService::TankID&, Common::EXIT_CODE, const QString&)),
                         SLOT(errorOccurredTank(const LevelGaugeService::TankID&, Common::EXIT_CODE, const QString&)), Qt::QueuedConnection);
        QObject::connect(tmp->tank.get(), SIGNAL(sendLogMsg(const LevelGaugeService::TankID&, Common::TDBLoger::MSG_CODE, const QString&)),
                         SLOT(sendLogMsgTank(const LevelGaugeService::TankID&, Common::TDBLoger::MSG_CODE, const QString &)), Qt::QueuedConnection);

        QObject::connect(tmp->tank.get(), SIGNAL(calculateStatuses(const LevelGaugeService::TankID&, const LevelGaugeService::TankStatusesList&)),
                         SLOT(calculateStatusesTank(const LevelGaugeService::TankID&, const LevelGaugeService::TankStatusesList&)), Qt::QueuedConnection);
        QObject::connect(tmp->tank.get(), SIGNAL(calculateIntakes(const LevelGaugeService::TankID&, const LevelGaugeService::IntakesList&)),
                         SLOT(calculateIntakesTank(const LevelGaugeService::TankID&, const LevelGaugeService::IntakesList&)), Qt::QueuedConnection);
        QObject::connect(this, SIGNAL(newStatuses(const LevelGaugeService::TankID&, const LevelGaugeService::TankStatusesList&)),
                         tmp->tank.get(), SLOT(newStatuses(const LevelGaugeService::TankID&, const LevelGaugeService::TankStatusesList&)), Qt::QueuedConnection);

        tmp->thread->start();

        _tanks.emplace(tankId, std::move(tmp));
    }

    return true;
}
