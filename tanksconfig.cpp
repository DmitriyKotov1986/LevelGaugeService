#include "tanksconfig.h"

using namespace LevelGaugeService;
using namespace Common;

static const float FLOAT_EPSILON = 0.0000001f;

TanksConfig::TanksConfig(const Common::DBConnectionInfo dbConnectionInfo, QObject* parent /* = nullptr */)
    : QObject{parent}
    , _dbConnectionInfo(dbConnectionInfo)
{
    qRegisterMetaType<LevelGaugeService::TankID>("TankID");
}

TanksConfig::~TanksConfig()
{
    _tanksConfig.clear();
}

bool TanksConfig::loadFromDB()
{
    Q_ASSERT(!_db.isOpen());

    //подключаемся к БД
    if (!connectToDB(_db, _dbConnectionInfo, "TANKS_CONFIG_DB"))
    {
        emit errorOccurred(EXIT_CODE::SQL_NOT_CONNECT, connectDBErrorString(_db));

        return false;
    };

    Q_ASSERT(_db.isOpen());

    //загружаем данные об АЗС
    const auto queryText =
            QString("SELECT "
                        "[ID], [AZSCode], [TankNumber], [TankName], [Type], [Mode], [Status] ,[Volume] ,[Diametr], "
                        "[Product], [ProductStatus], [LastMeasumentDateTime], [LastSaveDateTime], [LastIntakeDateTime], [ServiceDB], [TimeShift], "
                        "[DeltaVolume], [DeltaMass], [DeltaDensity], [DeltaHeight], [DeltaTemp], "
                        "[DeltaIntakeVolume], [DeltaIntakeMass], [DeltaIntakeDensity], [DeltaIntakeHeight], [DeltaIntakeTemp], "
                        "[IntakeDetectHeight], [PumpingOutDetectHeight] "
                    "FROM [dbo].[TanksInfo] "
                    "WHERE [Enabled] <> 0 ");

    _db.transaction();
    QSqlQuery query(_db);
    query.setForwardOnly(true);

    if (!query.exec(queryText))
    {
        _db.rollback();

        emit errorOccurred(EXIT_CODE::LOAD_CONFIG_ERR, QString("Cannot load tank configuration from DB: %1").arg(executeDBErrorString(_db, query)));

        return false;
    }

    class TankConfigLoadException
        : public std::runtime_error
    {
    public:
        explicit TankConfigLoadException(const QString& what)
            : std::runtime_error(what.toStdString())
        {}
    };

    while (query.next())
    {
        try
        {
            const auto recordID = query.value("ID").toULongLong();
            const auto AZSCode = query.value("AZSCode").toString();
            if (AZSCode.isEmpty())
            {
                 throw TankConfigLoadException(QString("Value [TanksInfo]/AZSCode cannot be empty. Record ID: %1").arg(recordID));
            }

            const auto tankNumber = query.value("TankNumber").toUInt();
            if (tankNumber == 0)
            {
                 throw TankConfigLoadException(QString("Value [TanksInfo]/TankNumber cannot be empty. Record ID: %1").arg(recordID));
            }
            const auto id = TankID(AZSCode, tankNumber);

            const QString name = query.value("TankName").toString();
            if (name.isEmpty())
            {
                 throw TankConfigLoadException(QString("Value [TanksInfo]/TankName cannot be empty. Record ID: %1").arg(recordID));
            }

            const auto totalVolume = query.value("Volume").toFloat();
            if (totalVolume <= FLOAT_EPSILON)
            {
                 throw TankConfigLoadException(QString("Value [TanksInfo]/Volume cannot be null or negative number. Record ID: %1").arg(recordID));
            }

            const auto diametr = query.value("Diametr").toFloat();
            if (diametr <= FLOAT_EPSILON)
            {
                 throw TankConfigLoadException(QString("Value [TanksInfo]/Diametr cannot be null or negative number. Record ID: %1").arg(recordID));
            }

            const auto lastSave = query.value("LastSaveDateTime").toDateTime();
            const auto lastMeasuments = query.value("LastMeasumentDateTime").toDateTime();
            const auto lastIntake = query.value("LastIntakeDateTime").toDateTime();

            const auto timeShift = query.value("TimeShift").toInt(); //cмещение времени относительно сервера

            TankConfig::Delta deltaMax;
            deltaMax.volume = query.value("DeltaVolume").toFloat();
            deltaMax.mass = query.value("DeltaMass").toFloat();
            deltaMax.density = query.value("DeltaDensity").toFloat();
            deltaMax.height = query.value("DeltaHeight").toFloat();
            deltaMax.temp = query.value("DeltaTemp").toFloat();

            if (!deltaMax.checkDelta())
            {
                throw TankConfigLoadException(QString("Value [TanksInfo]/Delta[Volume, Mass, Density, Height, Temp] cannot be null or negative number. Record ID: %1").arg(recordID));
            }

            TankConfig::Delta deltaIntake;
            deltaIntake.volume = query.value("DeltaIntakeVolume").toFloat();
            deltaIntake.mass = query.value("DeltaIntakeMass").toFloat();
            deltaIntake.density = query.value("DeltaIntakeDensity").toFloat();
            deltaIntake.height = query.value("DeltaIntakeHeight").toFloat();
            deltaIntake.temp = query.value("DeltaIntakeTemp").toFloat();

            if (!deltaIntake.checkDelta())
            {
                throw TankConfigLoadException(QString("Value [TanksInfo]/DeltaIntake[Volume, Mass, Density, Height, Temp] cannot be null or negative number. Record ID: %1").arg(recordID));
            }

            const auto deltaIntakeHeight = query.value("IntakeDetectHeight").toFloat();
            if (deltaIntakeHeight <= FLOAT_EPSILON)
            {
                 throw TankConfigLoadException(QString("Value [TanksInfo]/IntakeDetectHeight cannot be null or negative number. Record ID: %1").arg(recordID));
            }

            const auto deltaPumpingOutHeight = query.value("PumpingOutDetectHeight").toFloat();
            if (deltaPumpingOutHeight <= FLOAT_EPSILON)
            {
                 throw TankConfigLoadException(QString("Value [TanksInfo]/PumpingOutDetectHeight cannot be null or negative number. Record ID: %1").arg(recordID));
            }

            const TankConfig::Status status = TankConfig::intToStatus(query.value("Status").toUInt());
            const TankConfig::Mode mode = TankConfig::intToMode(query.value("Mode").toUInt());
            if (mode == TankConfig::Mode::UNDEFINE)
            {
                throw TankConfigLoadException(QString("Invalid value [TanksInfo]/Mode. Record ID: %1").arg(recordID));
            }

            const TankConfig::Type type = TankConfig::intToType(query.value("Type").toUInt());
            if (type == TankConfig::Type::UNDEFINE)
            {
                throw TankConfigLoadException(QString("Invalid value [TanksInfo]/Type. Record ID: %1").arg(recordID));
            }

            const auto serviceDB = query.value("ServiceDB").toString();
            if (serviceDB.isEmpty())
            {
                 throw TankConfigLoadException(QString("Value [TanksInfo]/ServiceDB cannot be empty. Record ID: %1").arg(recordID));
            }

            const auto product = query.value("Product").toString();
            if (product.isEmpty())
            {
                 throw TankConfigLoadException(QString("Value [TanksInfo]/Product cannot be empty. Record ID: %1").arg(recordID));
            }
            const TankConfig::ProductStatus productStatus= TankConfig::intToProductStatus(query.value("ProductStatus").toUInt());
            if (productStatus == TankConfig::ProductStatus::UNDEFINE)
            {
                throw TankConfigLoadException(QString("Invalid value [TanksInfo]/ProductStatus. Record ID: %1").arg(recordID));
            }



            auto tankConfig_p = std::make_unique<TankConfig>(id, name, totalVolume, diametr, timeShift, mode , type, deltaMax, deltaIntake,
                                                             deltaIntakeHeight, deltaPumpingOutHeight, status, serviceDB, product, productStatus,
                                                             lastMeasuments, lastSave, lastIntake);

            QObject::connect(tankConfig_p.get(), SIGNAL(lastMeasuments(const LevelGaugeService::TankID& id, const QDateTime& lastTime)),
                             SLOT(lastMeasuments(const LevelGaugeService::TankID& id, const QDateTime& lastTime)));
            QObject::connect(tankConfig_p.get(), SIGNAL(lastSave(const LevelGaugeService::TankID& id, const QDateTime& lastTime)),
                             SLOT(lastSave(const LevelGaugeService::TankID& id, const QDateTime& lastTime)));
            QObject::connect(tankConfig_p.get(), SIGNAL(lastIntake(const LevelGaugeService::TankID& id, const QDateTime& lastTime)),
                             SLOT(lastintake(const LevelGaugeService::TankID& id, const QDateTime& lastTime)));

            _tanksConfig.emplace(id, std::move(tankConfig_p));
        }
        catch (TankConfigLoadException& err)
        {
            emit sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("Cannot load tank configuration from DB. Tank skipped. Error: %1").arg(err.what()));
        }
    }

    if (!_db.commit())
    {
        _db.rollback();

        emit errorOccurred(EXIT_CODE::LOAD_CONFIG_ERR, commitDBErrorString(_db));

        return false;
    }

    emit sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Load tank configuration from DB is successfully. Total tanks: %1").arg(_tanksConfig.size()));

    return true;
}

TankConfig *TanksConfig::getTankConfig(const TankID &id) const
{
    const auto tanksConfig_it = _tanksConfig.find(id);
    if (tanksConfig_it == _tanksConfig.end())
    {
        return nullptr;
    }

    return tanksConfig_it->second.get();
}

TankIDList TanksConfig::getTanksID() const
{
    TankIDList result(_tanksConfig.size());

    std::transform(_tanksConfig.begin(), _tanksConfig.end(), result.begin(),
        [](const auto& item)
        {
            return item.first;
        });

    return result;
}

bool TanksConfig::isExist(const TankID &id) const
{
    return _tanksConfig.contains(id);
}

void TanksConfig::lastUpdate(const QString& fieldName, const TankID &id, const QDateTime &lastTime)
{
    Q_ASSERT(_db.isOpen());

    const auto queryText =
            QString("UPDATE [dbo].[TanksInfo] "
                    "SET [%1] = CAST('%2' AS DATETIME2) "
                    "WHERE [AZSCode] = %3 AND [TankNumber] = %4 ")
            .arg(fieldName)
            .arg(lastTime.toString(Common::DATETIME_FORMAT))
            .arg(id.levelGaugeCode())
            .arg(id.tankNumber());

    _db.transaction();
    QSqlQuery query(_db);

    if (!query.exec(queryText))
    {
        emit errorOccurred(EXIT_CODE::SQL_EXECUTE_QUERY_ERR, QString("Cannot update %1. AZSCode: %2. Tank: %3. Error: %4")
                           .arg(fieldName)
                           .arg(id.levelGaugeCode())
                           .arg(id.tankNumber())
                           .arg(executeDBErrorString(_db, query)));
        _db.rollback();

        return;
    }

    if (!_db.commit())
    {
        _db.rollback();

        emit errorOccurred(EXIT_CODE::LOAD_CONFIG_ERR, commitDBErrorString(_db));

        return;
    }
}

void TanksConfig::lastMeasuments(const TankID &id, const QDateTime &lastTime)
{
    lastUpdate("LastMeasumentDateTime", id, lastTime);
}

void TanksConfig::lastSave(const TankID &id, const QDateTime &lastTime)
{
    lastUpdate("LastSaveDateTime", id, lastTime);
}

void TanksConfig::lastIntake(const TankID &id, const QDateTime &lastTime)
{
    lastUpdate("LastIntakeDateTime", id, lastTime);
}
