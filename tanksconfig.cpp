#include "tanksconfig.h"

using namespace LevelGaugeService;
using namespace Common;

static const float FLOAT_EPSILON = 0.0000001f;
static const QString TANKS_CONFIG_DB_NAME = "TANKS_CONFIG_DB";

TanksConfig::TanksConfig(const Common::DBConnectionInfo& dbConnectionInfo, QObject* parent /* = nullptr */)
    : QObject{parent}
    , _dbConnectionInfo(dbConnectionInfo)
{
    qRegisterMetaType<LevelGaugeService::TankID>("TankID");
}

TanksConfig::~TanksConfig()
{
    _tanksConfig.clear();

    closeDB(_db);
}

bool TanksConfig::loadFromDB()
{
    Q_ASSERT(!_db.isOpen());

    //подключаемся к БД
    try
    {
        connectToDB(_db, _dbConnectionInfo, TANKS_CONFIG_DB_NAME);
    }
    catch (const SQLException& err)
    {
        emit errorOccurred(EXIT_CODE::SQL_NOT_CONNECT, err.what());

        return false;
    }

    Q_ASSERT(_db.isOpen());

    //загружаем данные об АЗС
    const auto queryText =
            QString("SELECT "
                        "[ID], [AZSCode], [TankNumber], [RemoteApplicantID], [RemoteObjectID], [RemoteTankID], [TankName], [RemoteBearerToken], [RemoteBaseURL], "
                        "[Type], [Mode], [Status] ,[Volume] ,[Diametr], "
                        "[Product], [ProductStatus], [LastMeasumentDateTime], [LastSaveDateTime], [LastSendDateTime], [LastIntakeDateTime], [LastSendIntakeDateTime], [TimeShift], "
                        "[DeltaVolume], [DeltaMass], [DeltaDensity], [DeltaHeight], [DeltaTemp], "
                        "[DeltaIntakeVolume], [DeltaIntakeMass], [DeltaIntakeDensity], [DeltaIntakeHeight], [DeltaIntakeTemp], "
                        "[IntakeDetectHeight], [PumpingOutDetectHeight] "
                    "FROM [dbo].[TanksInfo] "
                    "WHERE [Enabled] <> 0 ");
    try
    {
        transactionDB(_db);

        QSqlQuery query(_db);
        query.setForwardOnly(true);

        DBQueryExecute(_db, query, queryText);

        while (query.next())
        {
            class TankConfigLoadException
                : public std::runtime_error
            {
            public:
                explicit TankConfigLoadException(const QString& what)
                    : std::runtime_error(what.toStdString())
                {}
            };

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

                const auto name = query.value("TankName").toString();
                if (name.isEmpty())
                {
                     throw TankConfigLoadException(QString("Value [TanksInfo]/TankName cannot be empty. Record ID: %1").arg(recordID));
                }

                const auto remoteBearerToken = query.value("RemoteBearerToken").toString();
                if (remoteBearerToken.isEmpty() || remoteBearerToken == "UNDEFINED")
                {
                     throw TankConfigLoadException(QString("Value [TanksInfo]/RemoteBearerToken cannot be empty or UNDEFINED. Record ID: %1").arg(recordID));
                }

                const auto remoteBaseUrl = query.value("RemoteBaseURL").toUrl();
                auto tmpUrl = remoteBaseUrl; //исключаем ситуацию когда в качестве адреса указан только протокол. Например "https://"
                tmpUrl.setPath("/tmp");
                if (remoteBaseUrl.isEmpty() || !tmpUrl.isValid())
                {
                     throw TankConfigLoadException(QString("Value [TanksInfo]/RemoteBaseURL cannot be empty or no valid. Record ID: %1").arg(recordID));
                }

                const auto remoteApplicantId = query.value("RemoteApplicantID").toLongLong();
                if (remoteApplicantId == 0)
                {
                     throw TankConfigLoadException(QString("Value [TanksInfo]/RemoteApplicantID cannot be null. Record ID: %1").arg(recordID));
                }

                const auto remoteObjectId = query.value("RemoteObjectID").toLongLong();
                if (remoteObjectId == 0)
                {
                     throw TankConfigLoadException(QString("Value [TanksInfo]/RemoteObjectID cannot be null. Record ID: %1").arg(recordID));
                }

                const auto remoteTankId = query.value("RemoteTankId").toLongLong();
                if (remoteTankId == 0)
                {
                     throw TankConfigLoadException(QString("Value [TanksInfo]/RemoteTankID cannot be null. Record ID: %1").arg(recordID));
                }

                if (std::find_if(_tanksConfig.begin(),_tanksConfig.end(),
                    [remoteTankId](const auto& tankConfig)
                    {
                        return tankConfig.second->remoteTankId() == remoteTankId;
                    }) != _tanksConfig.end())
                {
                    throw TankConfigLoadException(QString("Value [TanksInfo]/RemoteTankID not unique. Record ID: %1").arg(recordID));
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
                const auto lastSend = query.value("LastSendDateTime").toDateTime();
                const auto lastSendIntake = query.value("LastSendIntakeDateTime").toDateTime();

                const auto timeShift = query.value("TimeShift").toInt(); //cмещение времени относительно сервера

                TankConfig::Delta deltaMax;
                deltaMax.volume = query.value("DeltaVolume").toFloat();
                deltaMax.mass = query.value("DeltaMass").toFloat();
                deltaMax.density = query.value("DeltaDensity").toFloat();
                deltaMax.height = query.value("DeltaHeight").toFloat();
                deltaMax.temp = query.value("DeltaTemp").toFloat();

                if (!deltaMax.check())
                {
                    throw TankConfigLoadException(QString("Value [TanksInfo]/Delta[Volume, Mass, Density, Height, Temp] cannot be null or negative number. Record ID: %1").arg(recordID));
                }

                TankConfig::Delta deltaIntake;
                deltaIntake.volume = query.value("DeltaIntakeVolume").toFloat();
                deltaIntake.mass = query.value("DeltaIntakeMass").toFloat();
                deltaIntake.density = query.value("DeltaIntakeDensity").toFloat();
                deltaIntake.height = query.value("DeltaIntakeHeight").toFloat();
                deltaIntake.temp = query.value("DeltaIntakeTemp").toFloat();

                if (!deltaIntake.check())
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

                auto tankConfig_p = std::make_unique<TankConfig>(id,
                                                                 remoteApplicantId,  remoteObjectId,  remoteTankId, name, remoteBearerToken, remoteBaseUrl,
                                                                 totalVolume, diametr, timeShift, mode , type,
                                                                 deltaMax, deltaIntake,
                                                                 deltaIntakeHeight, deltaPumpingOutHeight,
                                                                 status, product, productStatus);

                tankConfig_p->setLastMeasuments(lastMeasuments);
                tankConfig_p->setLastSave(lastSave);
                tankConfig_p->setLastIntake(lastIntake);
                tankConfig_p->setLastSend(lastSend);
                tankConfig_p->setLastSendIntake(lastSendIntake);

                QObject::connect(tankConfig_p.get(), SIGNAL(lastMeasuments(const LevelGaugeService::TankID&, const QDateTime&)),
                                 SLOT(lastMeasuments(const LevelGaugeService::TankID&, const QDateTime&)), Qt::DirectConnection);
                QObject::connect(tankConfig_p.get(), SIGNAL(lastSave(const LevelGaugeService::TankID&, const QDateTime&)),
                                 SLOT(lastSave(const LevelGaugeService::TankID&, const QDateTime&)), Qt::DirectConnection);
                QObject::connect(tankConfig_p.get(), SIGNAL(lastIntake(const LevelGaugeService::TankID&, const QDateTime&)),
                                 SLOT(lastIntake(const LevelGaugeService::TankID&, const QDateTime&)), Qt::DirectConnection);
                QObject::connect(tankConfig_p.get(), SIGNAL(lastSend(const LevelGaugeService::TankID&, const QDateTime&)),
                                 SLOT(lastSend(const LevelGaugeService::TankID&, const QDateTime&)), Qt::DirectConnection);
                QObject::connect(tankConfig_p.get(), SIGNAL(lastSendIntake(const LevelGaugeService::TankID&, const QDateTime&)),
                                 SLOT(lastSendIntake(const LevelGaugeService::TankID&, const QDateTime&)), Qt::DirectConnection);

                _tanksConfig.emplace(id, std::move(tankConfig_p));
            }
            catch (TankConfigLoadException& err)
            {
                emit sendLogMsg(TDBLoger::MSG_CODE::WARNING_CODE, QString("Cannot load tank configuration from DB. Tank skipped. Error: %1").arg(err.what()));
            }
        }

        commitDB(_db);
    }
    catch (const SQLException& err)
    {
        _db.rollback();

        emit errorOccurred(EXIT_CODE::LOAD_CONFIG_ERR, QString("Cannot load tank configuration from DB: %1").arg(err.what()));

        return false;
    }

    if (_tanksConfig.empty())
    {
        emit errorOccurred(EXIT_CODE::LOAD_CONFIG_ERR, "No tanks for work. Total tanks: 0");

        return false;
    }

    emit sendLogMsg(TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Load tank configuration from DB is successfully. Total tanks: %1").arg(_tanksConfig.size()));

    return true;
}

bool TanksConfig::checkID(const TankID &id) const
{
    return _tanksConfig.contains(id);
}

TankConfig* TanksConfig::getTankConfig(const TankID &id)
{
    auto tanksConfig_it = _tanksConfig.find(id);

    Q_ASSERT(tanksConfig_it != _tanksConfig.end());

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

quint64 TanksConfig::tanksCount() const
{
    return _tanksConfig.size();
}

void TanksConfig::lastUpdate(const QString& fieldName, const TankID &id, const QDateTime& lastTime)
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

    try
    {
        DBQueryExecute(_db, queryText);
    }
    catch (const SQLException& err)
    {      
        emit errorOccurred(EXIT_CODE::SQL_EXECUTE_QUERY_ERR, QString("Cannot update %1. AZSCode: %2. Tank: %3. Error: %4")
                           .arg(fieldName)
                           .arg(id.levelGaugeCode())
                           .arg(id.tankNumber())
                           .arg(err.what()));

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

void TanksConfig::lastSend(const TankID &id, const QDateTime &lastTime)
{
    lastUpdate("LastSendDateTime", id, lastTime);
}

void TanksConfig::lastSendIntake(const TankID &id, const QDateTime &lastTime)
{
    lastUpdate("LastSendIntakeDateTime", id, lastTime);
}
