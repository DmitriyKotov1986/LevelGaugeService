//Qt
#include <QSqlResult>
#include <QRandomGenerator64>
#include <QList>

#include "synchttpstatus.h"

using namespace LevelGaugeService;
using namespace Common;

static const QString CONNECTION_TO_DB_NAME = "SyncHTTPStatus";
static const QString SYNC_NAME = "SyncToHTTPStatus";

SyncHTTPStatus::SyncHTTPStatus(const Common::DBConnectionInfo& dbConnectionInfo, TanksConfig* tanksConfig, QObject *parent /* = nullptr */)
    : SyncImpl{parent}
    , _dbConnectionInfo(dbConnectionInfo)
    , _tanksConfig(tanksConfig)
{
}

std::optional<SyncHTTPStatus::CheckPackageData> SyncHTTPStatus::needCheckPackageFromDB(const QString& tableName)
{
    Q_ASSERT(_db.isOpen());

    const auto queryText =
        QString("SELECT [PackageID], [AZSCode], [TankNumber] "
                "FROM [%1] "
                "WHERE [PackageID] IS NOT NULL AND [SendStatus] IN (%2, %3, %4) AND [UpdateStatusDateTime] < CAST('%5' AS DATETIME2) ")
            .arg(tableName)
            .arg(static_cast<quint8>(SUNCSync::PackageProcessingStatus::PENDING))
            .arg(static_cast<quint8>(SUNCSync::PackageProcessingStatus::HTTP_ERROR))
            .arg(static_cast<quint8>(SUNCSync::PackageProcessingStatus::SEND_TO_SERVER))
            .arg(QDateTime::currentDateTime().addSecs(-(60 * 10)).toString(DATETIME_FORMAT));

    QList<CheckPackageData> uuids;

    try
    {
        transactionDB(_db);

        QSqlQuery query(_db);
        query.setForwardOnly(true);

        DBQueryExecute(_db, query, queryText);

        while (query.next())
        {
            CheckPackageData packageData;
            packageData.packageId = query.value("PackageID").toUuid();
            const auto AZSCode = query.value("AZSCode").toString();
            const auto tankNumber = query.value("TankNumber").toUInt();
            packageData.tankId = TankID(AZSCode, tankNumber);

            uuids.emplaceBack(std::move(packageData));
        }


        commitDB(_db);
    }
    catch (const SQLException& err)
    {
        _db.rollback();

        emit errorOccurred(SYNC_NAME, EXIT_CODE::SQL_EXECUTE_QUERY_ERR, err.what());        
    }

    emit sendLogMsg(SYNC_NAME, TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Find %1 unchecked status package").arg(uuids.size()));

    if (!uuids.empty())
    {
        const auto shift = QRandomGenerator64::global()->bounded(uuids.size());

        return *std::next(uuids.begin(), shift);
    }

    return {};
}

QString SyncHTTPStatus::tankFilter(qint64 applicantID) const
{
    const auto& applicant = _suncSyncs.at(applicantID);
    bool isFirst = true;
    QString result;
    for (const auto& tankId: applicant.tanksID)
    {
        if (!isFirst)
        {
            result += " OR ";
        }
        isFirst = false;

        result += QString("([AZSCode] = '%1' AND [TankNumber] = %2 AND [DateTime] > CAST('%3' AS DATETIME2))")
                .arg(tankId.levelGaugeCode())
                .arg(tankId.tankNumber())
                .arg(_tanksConfig->getTankConfig(tankId)->lastSend().toString(DATETIME_FORMAT));
    }

    return result;
}

void SyncHTTPStatus::sendNewStatusesFromDB(qint64 applicantID)
{
    Q_ASSERT(_db.isOpen());
    Q_ASSERT(_isStarted);

    const auto& applicant = _suncSyncs.at(applicantID);

    //т.к. приоритетное значение имеет сохранненные измерения - то сначала загружаем их
    const auto queryText =
        QString("SELECT TOP (1000) "
                    "[ID], [AZSCode], [TankNumber], [DateTime], [Volume], [Mass], [Density], [Height], [Temp], [AdditionFlag], [Status] "
                "FROM [TanksCalculate] "
                "WHERE (%1) AND [PackageID] IS NULL "
                "ORDER BY [DateTime] ")
            .arg(tankFilter(applicantID));

    IdList sendIdList; ///< список ИД записей, которые будут отправлены в текущем пакете
    std::unordered_map<qint64, std::list<SUNCSync::Measument>> measumentsData; //key - remotetankId
    LastSendDateTime lastSendDateTime;
    try
    {
        transactionDB(_db);

        QSqlQuery query(_db);
        query.setForwardOnly(true);

        DBQueryExecute(_db, query, queryText);

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

                const auto tankConfig = _tanksConfig->getTankConfig(id);
                const auto lastSendFromDB = query.value("DateTime").toDateTime();
                if (lastSendFromDB < tankConfig->lastSend())
                {
                    continue;
                }

                SUNCSync::Measument measument;
                measument.volume = query.value("Volume").toDouble();
                measument.volumeUnitType = SUNCSync::VolumeUnitType::CUBIC_DECIMETER;
                measument.mass = query.value("Mass").toDouble();
                measument.massUnitType = SUNCSync::MassUnitType::KILOGRAM;
                measument.density = query.value("Density").toDouble();
                measument.level = query.value("Height").toDouble();
                measument.levelUnitType = SUNCSync::LevelUnitType::MILLIMETER;
                measument.measurementDate = query.value("DateTime").toDateTime();
                measument.temperature = query.value("Temp").toDouble();
                measument.oilProductType = SUNCSync::stringToOilProductType(tankConfig->product());

                if (!measument.check())
                {
                    throw TankStatusLoadException(QString("Invalid value tank status from [TanksCalculate]. Data: %1. Record ID: %2").arg(measument.toString()).arg(recordID));
                }

                sendIdList.push_back(query.value("ID").toString());
                measumentsData[tankConfig->remoteTankId()].emplace_back(std::move(measument));
                lastSendDateTime.emplace(id, lastSendFromDB);

            }
            catch (TankStatusLoadException& err)
            {
                emit sendLogMsg(SYNC_NAME, TDBLoger::MSG_CODE::WARNING_CODE, QString("Cannot load tank status from DB. Record skipped. Error: %1").arg(err.what()));
            }
        } 

        commitDB(_db);
    }
    catch (const SQLException& err)
    {
        _db.rollback();

        emit errorOccurred(SYNC_NAME, EXIT_CODE::SQL_EXECUTE_QUERY_ERR, err.what());

        return;
    }

    if (measumentsData.empty())
    {
        emit sendLogMsg(SYNC_NAME, TDBLoger::MSG_CODE::INFORMATION_CODE, QString("No unsent statuses for applicant ID: %1. Skipped").arg(applicantID));

        return;
    }

    SUNCSync::TankIndicators data;
    data.packageId = QUuid::createUuid();

    for (const auto& tankMeasumentsData: measumentsData)
    {
        SUNCSync::TankMeasurements measuments;
        measuments.tankId = tankMeasumentsData.first;
        for (const auto& tankMeasumentData: tankMeasumentsData.second)
        {
            measuments.measuments.emplaceBack(std::move(tankMeasumentData));
        }
        data.tankMeasurements.emplaceBack(std::move(measuments));
    }

    const auto sendId = applicant.suncSync->sendSendTankIndicators(data);

    PackageInfo packageInfo;
    packageInfo.packageId = data.packageId;
    packageInfo.type = SUNCSync::RequestType::SEND_TANK_INDICATORS;
    packageInfo.lastSendDateTime = lastSendDateTime;

    _sendedRequest.emplace(std::move(sendId), std::move(packageInfo));

    updatePackageStatus(sendIdList, data.packageId, SUNCSync::PackageProcessingStatus::SEND_TO_SERVER);

    ++_sendedStatusesCount;

    emit sendLogMsg(SYNC_NAME, TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Send status package for applicant ID: %1. Package ID: %2. Record ID in [TanksCalculate]: %3")
                    .arg(applicantID)
                    .arg(data.packageId.toString())
                    .arg(sendIdList.join(',')));
}

void SyncHTTPStatus::updatePackageStatus(const IdList &idList, const QUuid& packageId, SUNCSync::PackageProcessingStatus status)
{
    Q_ASSERT(!packageId.isNull());
    Q_ASSERT(_db.isOpen());

    try
    {
        transactionDB(_db);

        const auto currentDateTime = QDateTime::currentDateTime().toString(DATETIME_FORMAT);

        for (const auto& id: idList)
        {
            const auto queryText =
                    QString("UPDATE [TanksCalculate] "
                            "SET [SendDateTime] = CAST('%1' AS DATETIME2), [UpdateStatusDateTime] = CAST('%2' AS DATETIME2), [PackageID] = '%3', [SendStatus] = %4 "
                            "WHERE [ID] = %5 ")
                    .arg(currentDateTime)
                    .arg(currentDateTime)
                    .arg(packageId.toString())
                    .arg(static_cast<quint8>(status))
                    .arg(id);


            QSqlQuery query(_db);

            DBQueryExecute(_db, query, queryText);
        }

        commitDB(_db);
    }
    catch (const SQLException& err)
    {       
        _db.rollback();

        emit errorOccurred(SYNC_NAME, EXIT_CODE::SQL_EXECUTE_QUERY_ERR, QString("Cannot create PackageID status. Package ID: %1. New status: %2. Records ID: %3. Error: %4")
                           .arg(packageId.toString())
                           .arg(SUNCSync::packageProcessingStatusToString(status))
                           .arg(idList.join(','))
                           .arg(err.what()));
    }
}

void SyncHTTPStatus::updatePackageStatus(const QUuid &packageId, SUNCSync::PackageProcessingStatus status, const QString &errorMessage)
{
    Q_ASSERT(!packageId.isNull());
    Q_ASSERT(_db.isOpen());

    const auto msg = errorMessage.toUtf8().toBase64();

    const auto queryText =
                QString("UPDATE [TanksCalculate] "
                        "SET [SendStatus] = %1, [UpdateStatusDateTime] = CAST('%2' AS DATETIME2), [ErrorText] = '%3' "
                        "WHERE [PackageID] = '%4' ")
                .arg(static_cast<quint8>(status))
                .arg(QDateTime::currentDateTime().toString(DATETIME_FORMAT))
                .arg(msg)
                .arg(packageId.toString());

    try
    {
        DBQueryExecute(_db, queryText);
    }
    catch (const SQLException& err)
    {
        emit errorOccurred(SYNC_NAME, EXIT_CODE::SQL_EXECUTE_QUERY_ERR, QString("Cannot update Package ID status. Package ID: %1. New status: %2. Error: %3")
                           .arg(packageId.toString())
                           .arg(SUNCSync::packageProcessingStatusToString(status))
                           .arg(err.what()));

    }
}

void SyncHTTPStatus::clearPackageStatus(const QUuid &packageId)
{
    Q_ASSERT(!packageId.isNull());
    Q_ASSERT(_db.isOpen());

    const auto queryText =
                QString("UPDATE [TanksCalculate] "
                        "SET [PackageID] = NULL "
                        "WHERE [PackageID] = '%1' ")
                .arg(packageId.toString());

    try
    {
        DBQueryExecute(_db, queryText);
    }
    catch (const SQLException& err)
    {
        emit errorOccurred(SYNC_NAME, EXIT_CODE::SQL_EXECUTE_QUERY_ERR, QString("Cannot clear [TanksCalculate]/Package ID. Package ID: %1. Error: %2")
                           .arg(packageId.toString())
                           .arg(err.what()));

    }
}

SyncHTTPStatus::~SyncHTTPStatus()
{
    stop();
}

void SyncHTTPStatus::start()
{
    Q_ASSERT(!_isStarted);

    for (const auto& tankId: _tanksConfig->getTanksID())
    {
        const auto tankConfig =  _tanksConfig->getTankConfig(tankId);

        auto suncSyncs_it = _suncSyncs.find(tankConfig->remoteApplicantId());
        if (suncSyncs_it != _suncSyncs.end())
        {
            suncSyncs_it->second.tanksID.push_back(tankId);

            continue;
        }

        ApplicantData applicant;
        applicant.tanksID.push_back(tankId);
        applicant.suncSync = std::make_unique<SUNCSync>(tankConfig->remoteBaseUrl(), tankConfig->remoteBearerToken());

        QObject::connect(applicant.suncSync.get(), SIGNAL(errorRequest(const QString&, LevelGaugeService::SUNCSync::PackageProcessingStatus, quint64)),
                                     SLOT(errorRequest(const QString&, LevelGaugeService::SUNCSync::PackageProcessingStatus, quint64)));
        QObject::connect(applicant.suncSync.get(), SIGNAL(sendLogMsg(Common::TDBLoger::MSG_CODE, const QString&, quint64)),
                                     SLOT(sendLogMsgHTTP(Common::TDBLoger::MSG_CODE, const QString&, quint64)));
        QObject::connect(applicant.suncSync.get(), SIGNAL(getPackageStatus(const LevelGaugeService::SUNCSync::PackageStatusInfo&, quint64)),
                                     SLOT(getPackageStatus(const LevelGaugeService::SUNCSync::PackageStatusInfo&, quint64)));
        QObject::connect(applicant.suncSync.get(), SIGNAL(sendTankIndicators(const LevelGaugeService::SUNCSync::TankIndicatorsInfo&, quint64)),
                                     SLOT(sendTankIndicators(const LevelGaugeService::SUNCSync::TankIndicatorsInfo&, quint64)));

        _suncSyncs.emplace(tankConfig->remoteApplicantId(), std::move(applicant));
    }

    try
    {
        connectToDB(_db, _dbConnectionInfo, QString("%1").arg(CONNECTION_TO_DB_NAME));
    }
    catch (const SQLException& err)
    {
        emit errorOccurred(SYNC_NAME, EXIT_CODE::SQL_NOT_CONNECT, err.what());

        return;
    }

    //check status
    Q_ASSERT(_checkStatusTimer == nullptr);
    _checkStatusTimer = new QTimer();

    QObject::connect(_checkStatusTimer, SIGNAL(timeout()), SLOT(checkPackage()));

    _checkStatusTimer->start(1000);

    //send Status
    Q_ASSERT(_sendStatusTimer == nullptr);
    _sendStatusTimer = new QTimer();

    QObject::connect(_sendStatusTimer, SIGNAL(timeout()), SLOT(sendNewStatuses()));

    _sendStatusTimer->start(60000);

    _isStarted = true;
}

void SyncHTTPStatus::stop()
{
    if (!_isStarted)
    {
        return;
    }

    _suncSyncs.clear();

    delete _sendStatusTimer;
    delete _checkStatusTimer;

    closeDB(_db);

    _isStarted = false;
}

void SyncHTTPStatus::sendNewStatuses()
{
    Q_ASSERT(_isStarted);

    if (_sendedStatusesCount != 0)
    {
        return;
    }

    for (const auto& applicant: _suncSyncs)
    {
        sendNewStatusesFromDB(applicant.first);
    }
}

void SyncHTTPStatus::sendCheckPackage(const CheckPackageData& packageData)
{
    Q_ASSERT(_isStarted);

    const auto tankConfig = _tanksConfig->getTankConfig(packageData.tankId);
    const auto sendId = _suncSyncs.at(tankConfig->remoteApplicantId()).suncSync->sendGetPackageStatus(packageData.packageId);

    PackageInfo packageInfo;
    packageInfo.packageId = packageData.packageId;
    packageInfo.type = SUNCSync::RequestType::GET_PACKAGE_STATUS;

    _sendedRequest.emplace(std::move(sendId), std::move(packageInfo));

    emit sendLogMsg(SYNC_NAME, TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Check status package on the server. Package ID: %1").arg(packageData.packageId.toString()));

    _isSendedCheckPackage = true;
}

void SyncHTTPStatus::sendLogMsgHTTP(Common::TDBLoger::MSG_CODE category, const QString &msg, quint64 id)
{
    emit sendLogMsg(SYNC_NAME, category, msg);
}

void SyncHTTPStatus::errorRequest(const QString &msg, LevelGaugeService::SUNCSync::PackageProcessingStatus status, quint64 id)
{
    if (!_isStarted)
    {
        return;
    }

    const auto sendedRequest_it = _sendedRequest.find(id);

    Q_ASSERT(sendedRequest_it != _sendedRequest.end());

    const auto& packageInfo = sendedRequest_it.value();

    switch (packageInfo.type)
    {
    case SUNCSync::RequestType::SEND_TANK_INDICATORS:
    {
        switch (status)
        {
        case SUNCSync::PackageProcessingStatus::INCORRECT_DATA_ERROR:
            updatePackageStatus(packageInfo.packageId, status, msg);

            break;
        case SUNCSync::PackageProcessingStatus::HTTP_ERROR:
            updatePackageStatus(packageInfo.packageId, status, msg);
            break;
        default:
            Q_ASSERT(false);
        }

        emit sendLogMsg(SYNC_NAME, TDBLoger::MSG_CODE::WARNING_CODE, QString("An unsuccessful attempt to send status data to the server. Package ID: %1. Status: %2. Message: %3")
                             .arg(packageInfo.packageId.toString())
                             .arg(SUNCSync::packageProcessingStatusToString(status))
                             .arg(msg));

        --_sendedStatusesCount;

        break;
    }
    case SUNCSync::RequestType::GET_PACKAGE_STATUS:
    {
        emit sendLogMsg(SYNC_NAME, TDBLoger::MSG_CODE::WARNING_CODE, QString("An unsuccessful check package status to the server. Package ID: %1. Status: %2. Message: %3")
                             .arg(packageInfo.packageId.toString())
                             .arg(SUNCSync::packageProcessingStatusToString(status))
                             .arg(msg));

        _isSendedCheckPackage = false;

        break;
    }

    default:
        Q_ASSERT(false);
    }

    _sendedRequest.erase(sendedRequest_it);
}

void SyncHTTPStatus::getPackageStatus(const SUNCSync::PackageStatusInfo& status, quint64 id)
{
    if (!_isStarted)
    {
        return;
    }

    const auto sendedRequest_it = _sendedRequest.find(id);

    Q_ASSERT(sendedRequest_it != _sendedRequest.end());

    const auto& packageInfo = sendedRequest_it.value();

    if (status.success)
    {
        updatePackageStatus(packageInfo.packageId, status.packageProcessingStatus, "");

        emit sendLogMsg(SYNC_NAME, TDBLoger::MSG_CODE::INFORMATION_CODE, QString("The package status has been successfully received from the server and chenged status to SUCCESS. Package ID: %1. Status: %2")
                        .arg(packageInfo.packageId.toString())
                        .arg(SUNCSync::packageProcessingStatusToString(status.packageProcessingStatus)));
    }
    else
    {
        //полное сообщение выглядит как "PackageId %1 wasn`t found.", но символ ` заменяеться ' на в QString и сравнение не срабатывает
        //поэтому срас=вниваем только половину фразы
        if (status.error.contains(QString("PackageId %1 wasn").arg(packageInfo.packageId.toString(QUuid::WithoutBraces)), Qt::CaseInsensitive))
        {
            clearPackageStatus(packageInfo.packageId);

            emit sendLogMsg(SYNC_NAME, TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Status package not found on server. Clear PackageID and will retry send statuses. Package ID: %1")
                        .arg(packageInfo.packageId.toString()));
        }
        else
        {
            updatePackageStatus(packageInfo.packageId, SUNCSync::PackageProcessingStatus::SERVER_ERROR, status.error);

            emit sendLogMsg(SYNC_NAME, TDBLoger::MSG_CODE::WARNING_CODE, QString("Failed to get the package status from the server and chenged status to SERVER_ERROR. Package ID: %1. Error: %2")
                        .arg(packageInfo.packageId.toString())
                        .arg(status.error));
        }
    }

    _sendedRequest.erase(sendedRequest_it);

    _isSendedCheckPackage = false;
}

void SyncHTTPStatus::sendTankIndicators(const SUNCSync::TankIndicatorsInfo& tankIndicators, quint64 id)
{
    Q_ASSERT(_isStarted);
    Q_ASSERT(_sendedStatusesCount != 0);

    const auto sendedRequest_it = _sendedRequest.find(id);

    Q_ASSERT(sendedRequest_it != _sendedRequest.end());

    const auto& packageInfo = sendedRequest_it.value();

    if (tankIndicators.success)
    {
        updatePackageStatus(packageInfo.packageId, SUNCSync::PackageProcessingStatus::PENDING, "");

        emit sendLogMsg(SYNC_NAME, TDBLoger::MSG_CODE::WARNING_CODE, QString("Package sended to the server successfully and chenged status to PENDING. Package ID: %1")
                    .arg(packageInfo.packageId.toString()));
    }
    else
    {
        updatePackageStatus(packageInfo.packageId, SUNCSync::PackageProcessingStatus::SERVER_ERROR, tankIndicators.error);

        emit sendLogMsg(SYNC_NAME, TDBLoger::MSG_CODE::WARNING_CODE, QString("Failed to sending package status to the server and chenged status to SERVER_ERROR. Package ID: %1. Error: %2")
                    .arg(packageInfo.packageId.toString())
                    .arg(tankIndicators.error));
    }

    for (const auto& lastSend: packageInfo.lastSendDateTime)
    {
        _tanksConfig->getTankConfig(lastSend.first)->setLastSend(lastSend.second);
    }

    _sendedRequest.erase(sendedRequest_it);

    --_sendedStatusesCount;
}

void SyncHTTPStatus::checkPackage()
{
    Q_ASSERT(_isStarted);

    if (_isSendedCheckPackage)
    {
        return;
    }

    const auto needCheckUuid = needCheckPackageFromDB("TanksCalculate");
    if (needCheckUuid.has_value())
    {
       sendCheckPackage(needCheckUuid.value());
       _checkStatusTimer->setInterval(1000);
    }
    else
    {
        _checkStatusTimer->setInterval(60000);
    }

    //далее ждем сигнала getPackageStatus(...)
}



