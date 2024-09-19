//Qt
#include <QUuid>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>

//My
#include "Common/common.h"

#include "suncsync.h"

using namespace LevelGaugeService;
using namespace Common;

//static const QString BASE_URL = "https://sunp-api.qoldau.kz";
//static const QString BASE_URL = "https://demo-sunp-api.qoldau.kz";
static const QString MEASUMENT_DATETIME_FORMAT = "yyyy-MM-ddThh:mm:ss.zzzZ";

//вспомогательные функции
QString SUNCSync::requestGuid()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

SUNCSync::PackageProcessingStatus SUNCSync::stringToPackageProcessingStatus(const QString &status)
{
    const auto statusLower = status.toLower();
    if (statusLower == "pending")
    {
        return PackageProcessingStatus::PENDING;
    }
    else if (statusLower == "undefined")
    {
        return PackageProcessingStatus::UNDEFINED;
    }
    else if (statusLower == "success")
    {
        return PackageProcessingStatus::SUCCESS;
    }
    else if (statusLower == "http_error")
    {
        return PackageProcessingStatus::HTTP_ERROR;
    }
    else if (statusLower == "incorrect_data_error")
    {
        return PackageProcessingStatus::INCORRECT_DATA_ERROR;
    }
    else if (statusLower == "error")
    {
        return PackageProcessingStatus::SERVER_ERROR;
    }
    else if (statusLower == "send_to_server")
    {
        return PackageProcessingStatus::SEND_TO_SERVER;
    }

    Q_ASSERT(false);

    return  PackageProcessingStatus::UNDEFINED;
}

QString SUNCSync::packageProcessingStatusToString(PackageProcessingStatus status)
{
    switch (status)
    {
    case PackageProcessingStatus::PENDING: return QString("PENDING");
    case PackageProcessingStatus::UNDEFINED: return QString("UNDEFINED");
    case PackageProcessingStatus::SUCCESS: return QString("SUCCESS");
    case PackageProcessingStatus::HTTP_ERROR: return QString("HTTP_ERROR");
    case PackageProcessingStatus::INCORRECT_DATA_ERROR: return QString("INCORRECT_DATA_ERROR");
    case PackageProcessingStatus::SERVER_ERROR: return QString("SERVER_ERROR");
    case PackageProcessingStatus::SEND_TO_SERVER: return QString("SEND_TO_SERVER");
    default:
        Q_ASSERT(false);
    }

    return QString("UNDEFINED");
}

SUNCSync::ObjectType SUNCSync::stringToObjectType(const QString &type)
{
    const auto typeLower = type.toLower();
    if (typeLower == "petrolstation")
    {
        return SUNCSync::ObjectType::PETROL_STATION;
    }
    else if (typeLower == "refinery")
    {
        return SUNCSync::ObjectType::REFINERY;
    }
    else if (typeLower == "refinerylow")
    {
        return SUNCSync::ObjectType::REFINERY_LOW;
    }
    else if (typeLower == "oilbase")
    {
        return SUNCSync::ObjectType::OIL_BASE;
    }

    return SUNCSync::ObjectType::UNDEFINED;
}

QString SUNCSync::oilProductTypeToString(OilProductType type)
{
    switch (type)
    {
    case OilProductType::UNDEFINED: return "Undefined";
    case OilProductType::AI80: return "AI80";
    case OilProductType::AI92: return "AI92";
    case OilProductType::AI95: return "AI95";
    case OilProductType::AI96: return "AI96";
    case OilProductType::AI98: return "AI98";      
    case OilProductType::DTZ: return "DTZ";
    case OilProductType::DTL: return "DTL";
    case OilProductType::M100: return "M100";
    case OilProductType::TS1: return "TS1";
    case OilProductType::DTZK4: return "DTZK4";
    case OilProductType::DTM: return "DTM";
    case OilProductType::AI95K4: return "AI95K4";
    case OilProductType::AI95K5: return "AI95K5";
    case OilProductType::AI98K4: return "AI98K4";
    case OilProductType::DTEK5: return "DTEK5";
    case OilProductType::AI93: return "AI93";
    case OilProductType::AI98K5: return "AI98K5";
    case OilProductType::DTEK2: return "DTEK2";
    case OilProductType::ZM40: return "ZM40";
    case OilProductType::AI92K4: return "ZM100";
    case OilProductType::AI96K4: return "AI96K4";
    case OilProductType::DTE: return "DTE";
    case OilProductType::HYDRAZINE: return "HYDRAZINE";
    case OilProductType::NPD: return "NPD";
    case OilProductType::AI76: return "AI76";
    case OilProductType::DTA: return "DTA";
    case OilProductType::DTZK5: return "DTZK5";
    case OilProductType::DTLK4: return "DTLK4";
    case OilProductType::DTLK5: return "DTLK5";
    case OilProductType::G100: return "G100";
    case OilProductType::G95: return "G95";
    case OilProductType::DTZK2: return "DTZK2";
    case OilProductType::DTEK4: return "DTEK4";
    case OilProductType::DTAK4: return "DTAK4";
    case OilProductType::DT: return "DT";
    case OilProductType::DTAK2: return "DTAK2";
    case OilProductType::AI91: return "AI91";
    case OilProductType::UNKNOWN: return "UNKNOWN";
    case OilProductType::AI100: return "AI100";
    case OilProductType::DTAK5: return "DTAK5";
    case OilProductType::NEFRAS: return "NEFRAS";
    default:
        Q_ASSERT(false);
    }

    return "Undefined";
}

SUNCSync::OilProductType SUNCSync::stringToOilProductType(const QString& type)
{
    const auto typeLower = type.toLower();
    if (typeLower == "80")
    {
        return SUNCSync::OilProductType::AI80;
    }
    else if (typeLower == "92")
    {
        return SUNCSync::OilProductType::AI92;
    }
    else if (typeLower == "95")
    {
        return SUNCSync::OilProductType::AI95;
    }
    else if (typeLower == "96")
    {
        return SUNCSync::OilProductType::AI96;
    }
    else if (typeLower == "98")
    {
        return SUNCSync::OilProductType::AI98;
    }
    else if (typeLower == "100")
    {
        return SUNCSync::OilProductType::AI100;
    }
    else if (typeLower == "dt")
    {
        return SUNCSync::OilProductType::DT;
    }
    else if (typeLower == "dtl")
    {
        return SUNCSync::OilProductType::DTL;
    }
    else if (typeLower == "dtz")
    {
        return SUNCSync::OilProductType::DTZ;
    }

    qWarning() << "Incorrect oil product type: " << type;

    return SUNCSync::OilProductType::UNDEFINED;
}

SUNCSync::DeviceType stringToDeviceType(const QString& type)
{    
    const auto typeLower = type.toLower();
    if (typeLower == "undefined")
    {
        return SUNCSync::DeviceType::UNDEFINED;
    }
    else if (typeLower == "flowmeteroutputavto")
    {
        return SUNCSync::DeviceType::FLOWMETER_OUTPUT_AVTO;
    }
    else if (typeLower == "flowmeteroutputzhd")
    {
        return SUNCSync::DeviceType::FLOWMETER_OUTPUT_ZHD;
    }
    else if (typeLower == "flowmeteroutputtube")
    {
        return SUNCSync::DeviceType::FLOWMETER_OUTPUT_TUBE;
    }
    else if (typeLower == "flowmeterinputavto")
    {
        return SUNCSync::DeviceType::FLOWMETER_INPUT_AVTO;
    }
    else if (typeLower == "flowmeterinputzhd")
    {
        return SUNCSync::DeviceType::FLOWMETER_INPUT_ZHD;
    }
    else if (typeLower == "flowmeterinputtube")
    {
        return SUNCSync::DeviceType::FLOWMETER_INPUT_TUBE;
    }
    else if (typeLower == "electronicscale")
    {
        return SUNCSync::DeviceType::ELECTRONIC_SCALE;
    }
    else if (typeLower == "transfer")
    {
        return SUNCSync::DeviceType::TRANSFER;
    }

    qWarning() << "Incorrect device type: " << type;

    return SUNCSync::DeviceType::UNDEFINED;
}

QString SUNCSync::massUnitTypeToString(SUNCSync::MassUnitType type)
{
    switch (type)
    {
    case SUNCSync::MassUnitType::UNDEFINED: return "Undefined";
    case SUNCSync::MassUnitType::KILOGRAM: return "Kilogram";
    case SUNCSync::MassUnitType::QUINTAL: return "Quintal";
    case SUNCSync::MassUnitType::TON: return "Ton";
    default:
        Q_ASSERT(false);
    }

    return "Undefine";
}

QString SUNCSync::volumeUnitTypeToString(SUNCSync::VolumeUnitType type)
{
    switch (type)
    {
    case SUNCSync::VolumeUnitType::UNDEFINED: return "Undefined";
    case SUNCSync::VolumeUnitType::CUBIC_CENTIMETER: return "Cubiccentimeter";
    case SUNCSync::VolumeUnitType::CUBIC_DECIMETER: return "Cubicdecimeter";
    case SUNCSync::VolumeUnitType::CUBIC_METER: return "Cubicmeter";
    default:
        Q_ASSERT(false);
    }

    return "Undefine";
}

QString SUNCSync::levelUnitTypeToString(SUNCSync::LevelUnitType type)
{
    switch (type)
    {
    case SUNCSync::LevelUnitType::UNDEFINED: return "Undefined";
    case SUNCSync::LevelUnitType::CENTIMETER: return "Centimeter";
    case SUNCSync::LevelUnitType::MILLIMETER: return "Millimeter";
    case SUNCSync::LevelUnitType::METER: return "Meter";
    default:
        Q_ASSERT(false);
    }

    return "Undefine";
}

QString SUNCSync::transferOperationTypeToString(TransferOperationType type)
{
    switch (type)
    {
    case SUNCSync::TransferOperationType::UNDEFINED: return "Undefined";
    case SUNCSync::TransferOperationType::INCOME: return "Income";
    case SUNCSync::TransferOperationType::OUTCOME: return "Outcome";
    default:
        Q_ASSERT(false);
    }

    return "Undefine";
}

LevelGaugeService::SUNCSync::SUNCSync(const QUrl& baseUrl, const QString& remoteBearerToken, QObject *parent /* = nullptr */)
    : QObject{parent}
    , _query(HTTPSSLQuery::ProxyList())
    , _remoteBearerToken(remoteBearerToken)
    , _baseUrl(baseUrl)
{
    Q_ASSERT(!_remoteBearerToken.isEmpty());
    Q_ASSERT(!_baseUrl.isEmpty() && _baseUrl.isValid());

    qRegisterMetaType<LevelGaugeService::SUNCSync::PackageStatusInfo>("LevelGaugeService::SUNCSync::PackageStatusInfo");
    qRegisterMetaType<LevelGaugeService::SUNCSync::ApplicantDataInfo>("LevelGaugeService::SUNCSync::ApplicantDataInfo");
    qRegisterMetaType<LevelGaugeService::SUNCSync::TankIndicatorsInfo>("LevelGaugeService::SUNCSync::TankIndicatorsInfo");
    qRegisterMetaType<LevelGaugeService::SUNCSync::TankTransfersInfo>("LevelGaugeService::SUNCSync::TankTransfersInfo");
    qRegisterMetaType<LevelGaugeService::SUNCSync::FlowmeterOutputIndicatorsInfo>("LevelGaugeService::SUNCSync::FlowmeterOutputIndicatorsInfo");
    qRegisterMetaType<LevelGaugeService::SUNCSync::FlowmeterInputIndicatorsInfo>("LevelGaugeService::SUNCSync::FlowmeterInputIndicatorsInfo");

    _headers.insert("Content-Type", "application/json");

    QObject::connect(&_query, SIGNAL(getAnswer(const QByteArray&, quint64)), SLOT(getAnswerHTTP(const QByteArray&, quint64)));
    QObject::connect(&_query, SIGNAL(errorOccurred(QNetworkReply::NetworkError, quint64, const QString&, quint64)),
                             SLOT(errorOccurredHTTP(QNetworkReply::NetworkError, quint64, const QString&, quint64)));
    QObject::connect(&_query, SIGNAL(sendLogMsg(Common::TDBLoger::MSG_CODE, const QString&, quint64)),
                             SLOT(sendLogMsgHTTP(Common::TDBLoger::MSG_CODE, const QString&, quint64)));
}

SUNCSync::~SUNCSync()
{
}

quint64 SUNCSync::sendGetPackageStatus(const QUuid& packageId)
{
    if (packageId.isNull())
    {
        const auto msg = QString("GetPackageStatus: Package ID cannot be null");
        const auto sendId = HTTPSSLQuery::getId();
        QTimer::singleShot(0, this, [this, msg, sendId](){ emit errorRequest(msg, PackageProcessingStatus::INCORRECT_DATA_ERROR, sendId); });

        return sendId;
    }

    QJsonObject data;
    data.insert("requestGuid", requestGuid());
    data.insert("packageId", packageId.toString(QUuid::WithoutBraces));

    QJsonDocument body(data);

    auto headers(_headers);
    headers.emplace("Authorization", _remoteBearerToken.toUtf8());

    auto url = _baseUrl;
    url.setPath("/Provider/GetPackageStatus");

    const auto id = _query.send(url,
                                HTTPSSLQuery::RequestType::POST,
                                body.toJson(QJsonDocument::Compact),
                                headers);

    _requests.insert(id, RequestType::GET_PACKAGE_STATUS);

    return id;
}

quint64 SUNCSync::sendGetApplicantData(const QString& bin)
{
/*
{
   "requestGuid": "71dEA85a-Ad44-4eB7-24B4-77CAB0eEc781",
   "bin": "string"
}
*/
    if (bin.isEmpty())
    {
        const auto msg = QString("GetApplicantData: BIN cannot be empty");
        const auto sendId = HTTPSSLQuery::getId();
        QTimer::singleShot(0, this, [this, msg, sendId](){ emit errorRequest(msg, PackageProcessingStatus::INCORRECT_DATA_ERROR, sendId); });

        return sendId;
    }


    QJsonObject data;
    data.insert("requestGuid", requestGuid());
    data.insert("bin", bin);

    QJsonDocument body(data);

    auto url = _baseUrl;
    url.setPath("/Provider/GetApplicantData");

    const auto id = _query.send(url,
                                HTTPSSLQuery::RequestType::GET,
                                body.toJson(QJsonDocument::Compact),
                                _headers);

    _requests.insert(id, RequestType::GET_APPLICANT_DATA);

    return id;
}

quint64 SUNCSync::sendSendTankIndicators(const TankIndicators& tankIndicators)
{
/*
{
  "requestGuid": "74FC31A1-afdF-6a9f-DFd8-Bdd1eF0E79f5",
  "packageId": "3eD5b6CA-630F-fc76-FF0E-733dB3Fd02Ce",
  "tanksMeasurements": [
    {
      "tankId": 264938628484000000,
      "measurements": [
        {
          "measurementDate": "2024-05-24T11:48:27.942Z",
          "mass": 10000,
          "massUnitType": "Kilogram",
          "volume": 10000,
          "volumeUnitType": "CubicMeter",
          "level": 500,
          "levelUnitType": "Millimeter",
          "density": 700,
          "temperature": 10,
          "oilProductType": "AI80",
          "tankStatus": "Sucks"
        }
      ]
    }
  ]
}
*/
    if (tankIndicators.packageId.isNull())
    { 
        const auto msg = QString("SendTankIndicators: Package ID cannot be null");
        const auto sendId = HTTPSSLQuery::getId();
        QTimer::singleShot(0, this, [this, msg, sendId](){ emit errorRequest(msg, PackageProcessingStatus::INCORRECT_DATA_ERROR, sendId); });

        return sendId;
    }

    QJsonObject data;
    data.insert("requestGuid", requestGuid());
    data.insert("packageId", tankIndicators.packageId.toString(QUuid::WithoutBraces));

    QJsonArray JSONTanksMeasurements;
    for(const auto& tanksMeasurement: tankIndicators.tankMeasurements)
    {
        QJsonObject JSONTanksMeasurement;
        JSONTanksMeasurement.insert("tankId", static_cast<qint64>(tanksMeasurement.tankId));

        QJsonArray JSONMeasurements;

        for (const auto& measurement: tanksMeasurement.measuments)
        {
            if (!measurement.check())
            {
                const auto msg = QString("SendTankIndicators: Incorrect data. Data: %1").arg(measurement.toString());
                const auto sendId = HTTPSSLQuery::getId();
                QTimer::singleShot(0, this, [this, msg, sendId](){ emit errorRequest(msg, PackageProcessingStatus::INCORRECT_DATA_ERROR, sendId); });

                return sendId;
            }

            QJsonObject JSONMeasument;

            JSONMeasument.insert("measurementDate", measurement.measurementDate.toString(MEASUMENT_DATETIME_FORMAT));
            JSONMeasument.insert("mass", measurement.mass);
            JSONMeasument.insert("massUnitType", massUnitTypeToString(measurement.massUnitType));
            JSONMeasument.insert("volume", measurement.volume);
            JSONMeasument.insert("volumeUnitType", volumeUnitTypeToString(measurement.volumeUnitType));
            JSONMeasument.insert("level", measurement.level);
            JSONMeasument.insert("levelUnitType", levelUnitTypeToString(measurement.levelUnitType));
            JSONMeasument.insert("density", measurement.density);
            JSONMeasument.insert("temperature", measurement.temperature);
            JSONMeasument.insert("oilProductType", oilProductTypeToString(measurement.oilProductType));

            JSONMeasurements.push_back(JSONMeasument);
        }

        JSONTanksMeasurement.insert("measurements", JSONMeasurements);

        JSONTanksMeasurements.push_back(JSONTanksMeasurement);
    }


    data.insert("tanksMeasurements", JSONTanksMeasurements);

    QJsonDocument body(data);

    auto headers(_headers);
    headers.emplace("Authorization", _remoteBearerToken.toUtf8());

    auto url = _baseUrl;
    url.setPath("/Tank/SendTankIndicators");

    const auto id = _query.send(url,
                                HTTPSSLQuery::RequestType::POST,
                                body.toJson(QJsonDocument::Compact),
                                headers);

    _requests.insert(id, RequestType::SEND_TANK_INDICATORS);

    return id;
}

quint64 SUNCSync::sendSendTankTransfers(const TanksTransfers &tanksTransfers)
{
    /*
    {
      "requestGuid": "E8abFfDA-eA0C-d07f-F6F6-dbE5B7c522AA",
      "packageId": "e1De4fa5-bbbE-bAb3-DFF1-47E62bD5d4a8",
      "tanksTransfers": [
        {
          "tankId": 0,
          "transfers": [
            {
              "startDate": "2024-08-14T08:03:32.952Z",
              "endDate": "2024-08-14T08:03:32.952Z",
              "levelStart": 0,
              "levelEnd": 0,
              "levelUnitType": "Millimeter",
              "massStart": 0,
              "massEnd": 0,
              "massUnitType": "Kilogram",
              "volumeStart": 0,
              "volumeEnd": 0,
              "volumeUnitType": "CubicMeter",
              "operationType": "Income",
              "oilProductType": "AI76"
            }
          ]
        }
      ]
    }
    */
    if (tanksTransfers.packageId.isNull())
    {
        const auto msg = QString("SendTankTransfers: Package ID cannot be null");
        const auto sendId = HTTPSSLQuery::getId();
        QTimer::singleShot(0, this, [this, msg, sendId](){ emit errorRequest(msg, PackageProcessingStatus::INCORRECT_DATA_ERROR, sendId); });

        return sendId;
    }

    QJsonObject data;
    data.insert("requestGuid", requestGuid());
    data.insert("packageId", tanksTransfers.packageId.toString(QUuid::WithoutBraces));

    QJsonArray JSONTanksTransfers;
    for(const auto& tankIntakes: tanksTransfers.tankTransfers)
    {
        QJsonObject JSONTankTransfers;
        JSONTankTransfers.insert("tankId", static_cast<qint64>(tankIntakes.tankId));

        QJsonArray JSONTransfers;

        for (const auto& transfer: tankIntakes.transfers)
        {
            if (!transfer.check())
            {
                const auto msg = QString("SendTankTransfers: Incorrect data. Data: %1").arg(transfer.toString());
                const auto sendId = HTTPSSLQuery::getId();
                QTimer::singleShot(0, this, [this, msg, sendId](){ emit errorRequest(msg, PackageProcessingStatus::INCORRECT_DATA_ERROR, sendId); });

                return sendId;
            }

            QJsonObject JSONTransfer;
            JSONTransfer.insert("startDate", transfer.startDate.toString(MEASUMENT_DATETIME_FORMAT));
            JSONTransfer.insert("endDate", transfer.endDate.toString(MEASUMENT_DATETIME_FORMAT));
            JSONTransfer.insert("levelStart", transfer.levelStart);
            JSONTransfer.insert("levelEnd", transfer.levelEnd);
            JSONTransfer.insert("levelUnitType", levelUnitTypeToString(transfer.levelUnitType));
            JSONTransfer.insert("massStart", transfer.massStart);
            JSONTransfer.insert("massEnd", transfer.massEnd);
            JSONTransfer.insert("massUnitType", massUnitTypeToString(transfer.massUnitType));
            JSONTransfer.insert("volumeStart", transfer.volumeStart);
            JSONTransfer.insert("volumeEnd", transfer.volumeEnd);
            JSONTransfer.insert("volumeUnitType", volumeUnitTypeToString(transfer.volumeUnitType));
            JSONTransfer.insert("operationType", transferOperationTypeToString(tankIntakes.operationType));
            JSONTransfer.insert("oilProductType", oilProductTypeToString(transfer.oilProductType));

            JSONTransfers.push_back(JSONTransfer);
        }

        JSONTankTransfers.insert("transfers", JSONTransfers);

        JSONTanksTransfers.push_back(JSONTankTransfers);
    }


    data.insert("tanksTransfers", JSONTanksTransfers);

    QJsonDocument body(data);

    auto headers(_headers);
    headers.emplace("Authorization", _remoteBearerToken.toUtf8());

    auto url = _baseUrl;
    url.setPath("/Tank/SendTankTransfers");

    const auto id = _query.send(url,
                                HTTPSSLQuery::RequestType::POST,
                                body.toJson(QJsonDocument::Compact),
                                headers);

    _requests.insert(id, RequestType::SEND_TANK_INDICATORS);

    return id;
}

quint64 SUNCSync::sendSendFlowmeterOutputIndicators(const FlowmeterOutputIndicators &flowmeterOutputIndicators)
{
/*
{
  "requestGuid": "C46A7db8-fe50-ab15-c3c8-232FbaCb422a",
  "packageId": "fae65BDa-0a7B-301D-DECE-d2eBCDA5B56e",
  "flowmetersOutputMeasurements": [
    {
      "deviceId": 264938643993000000,
      "measurements": [
        {
          "measurementDate": "2024-05-24T12:20:02.635Z",
          "totalMass": 100,
          "flowMass": 100,
          "totalVolume": 100,
          "currentDensity": 50,
          "currentTemperature": 5,
          "oilProductType": "AI80"
        }
      ]
    }
  ]
}
*/
    if (flowmeterOutputIndicators.packageId.isNull())
    {
        const auto msg = QString("SendFlowmeterOutputIndicators: Package ID cannot be null");
        const auto sendId = HTTPSSLQuery::getId();
        QTimer::singleShot(0, this, [this, msg, sendId](){ emit errorRequest(msg, PackageProcessingStatus::INCORRECT_DATA_ERROR, sendId); });

        return sendId;
    }

    QJsonObject data;
    data.insert("requestGuid", requestGuid());
    data.insert("packageId", flowmeterOutputIndicators.packageId.toString(QUuid::WithoutBraces));

    QJsonArray JSONTanksMeasurements;
    for(const auto& flowmeterOutputMeasurement: flowmeterOutputIndicators.flowmeterOutputMeasurements)
    {
        QJsonObject JSONTanksMeasurement;
        JSONTanksMeasurement.insert("deviceId", static_cast<qint64>(flowmeterOutputMeasurement.deviceId));

        QJsonArray JSONMeasurements;

        for (const auto& measurement: flowmeterOutputMeasurement.flowmeterOutputMeasurementData)
        {
            if (!measurement.check())
            {
                const auto msg = QString("SendFlowmeterOutputIndicators: Incorrect date. Data: %1").arg(measurement.toString());
                const auto sendId = HTTPSSLQuery::getId();
                QTimer::singleShot(0, this, [this, msg, sendId](){ emit errorRequest(msg, PackageProcessingStatus::INCORRECT_DATA_ERROR, sendId); });

                return sendId;
            }

            QJsonObject JSONMeasument;

            JSONMeasument.insert("measurementDate", measurement.measurementDate.toString(MEASUMENT_DATETIME_FORMAT));
            JSONMeasument.insert("totalMass", measurement.totalMass);
            JSONMeasument.insert("flowMass", measurement.flowMass);
            JSONMeasument.insert("totalVolume", measurement.totalVolume);
            JSONMeasument.insert("currentDensity", measurement.currentDensity);
            JSONMeasument.insert("currentTemperature", measurement.currentTemperature);
            JSONMeasument.insert("oilProductType", oilProductTypeToString(measurement.oilProductType));

            JSONMeasurements.push_back(JSONMeasument);
        }

        JSONTanksMeasurement.insert("flowmetersOutputMeasurements", JSONMeasurements);

        JSONTanksMeasurements.push_back(JSONTanksMeasurement);
    }

    data.insert("tanksMeasurements", JSONTanksMeasurements);

    QJsonDocument body(data);

    auto headers(_headers);
    headers.emplace("Authorization", _remoteBearerToken.toUtf8());

    auto url = _baseUrl;
    url.setPath("/Device/SendFlowmeterOutputIndicators");

    const auto id = _query.send(url,
                                HTTPSSLQuery::RequestType::POST,
                                body.toJson(QJsonDocument::Compact),
                                headers);

    _requests.insert(id, RequestType::SEND_FLOWERS_OUTPUT_INDICATOR);

    return id;
}

quint64 SUNCSync::sendSendFlowmeterInputIndicators(const FlowmeterInputIndicators &flowmeterInputIndicators)
{
    /*
    {
      "requestGuid": "C46A7db8-fe50-ab15-c3c8-232FbaCb422a",
      "packageId": "fae65BDa-0a7B-301D-DECE-d2eBCDA5B56e",
      "flowmetersInputMeasurements": [
        {
          "deviceId": 264938643993000000,
          "measurements": [
            {
              "measurementDate": "2024-05-24T12:20:02.635Z",
              "totalMass": 100,
              "flowMass": 100,
              "totalVolume": 100,
              "currentDensity": 50,
              "currentTemperature": 5,
              "oilProductType": "AI80"
            }
          ]
        }
      ]
    }
    */

    if (flowmeterInputIndicators.packageId.isNull())
    {
        const auto msg = QString("SendFlowmeterInputIndicators: Package ID cannot be empty");
        const auto sendId = HTTPSSLQuery::getId();
        QTimer::singleShot(0, this, [this, msg, sendId](){ emit errorRequest(msg, PackageProcessingStatus::INCORRECT_DATA_ERROR, sendId); });

        return sendId;
    }

    QJsonObject data;
    data.insert("requestGuid", requestGuid());
    data.insert("packageId", flowmeterInputIndicators.packageId.toString(QUuid::WithoutBraces));

    QJsonArray JSONTanksMeasurements;
    for(const auto& flowmeterOutputMeasurement: flowmeterInputIndicators.flowmeterInputMeasurements)
    {
        QJsonObject JSONTanksMeasurement;
        JSONTanksMeasurement.insert("deviceId", static_cast<qint64>(flowmeterOutputMeasurement.deviceId));

        QJsonArray JSONMeasurements;

        for (const auto& measurement: flowmeterOutputMeasurement.flowmeterInputMeasurementData)
        {
            if (!measurement.check())
            {
                const auto msg = QString("SendFlowmeterInputIndicators: Incorrect date. Data: %1").arg(measurement.toString());
                const auto sendId = HTTPSSLQuery::getId();
                QTimer::singleShot(0, this, [this, msg, sendId](){ emit errorRequest(msg, PackageProcessingStatus::INCORRECT_DATA_ERROR, sendId); });

                return sendId;
            }

            QJsonObject JSONMeasument;

            JSONMeasument.insert("measurementDate", measurement.measurementDate.toString(MEASUMENT_DATETIME_FORMAT));
            JSONMeasument.insert("totalMass", measurement.totalMass);
            JSONMeasument.insert("flowMass", measurement.flowMass);
            JSONMeasument.insert("totalVolume", measurement.totalVolume);
            JSONMeasument.insert("currentDensity", measurement.currentDensity);
            JSONMeasument.insert("currentTemperature", measurement.currentTemperature);
            JSONMeasument.insert("oilProductType", oilProductTypeToString(measurement.oilProductType));

            JSONMeasurements.push_back(JSONMeasument);
        }

        JSONTanksMeasurement.insert("flowmetersInputMeasurements", JSONMeasurements);

        JSONTanksMeasurements.push_back(JSONTanksMeasurement);
    }

    data.insert("tanksMeasurements", JSONTanksMeasurements);

    QJsonDocument body(data);

    auto headers(_headers);
    headers.emplace("Authorization", _remoteBearerToken.toUtf8());

    auto url = _baseUrl;
    url.setPath("/Device/SendFlowmeterInputIndicators");

    const auto id = _query.send(url,
                                HTTPSSLQuery::RequestType::POST,
                                body.toJson(QJsonDocument::Compact),
                                headers);

    _requests.insert(id, RequestType::SEND_FLOWERS_OUTPUT_INDICATOR);

    return id;
}

void SUNCSync::getAnswerHTTP(const QByteArray &answer, quint64 id)
{
    const auto it_requests = _requests.constFind(id);

    Q_ASSERT(it_requests != _requests.end());

    const auto type = it_requests.value();

    switch (type)
    {
    case SUNCSync::RequestType::GET_PACKAGE_STATUS:
        parseGetPackageStatus(answer, id);
        break;
    case SUNCSync::RequestType::GET_APPLICANT_DATA:
        parseGetApplicantData(answer, id);
        break;
    case SUNCSync::RequestType::SEND_TANK_INDICATORS:
        parseSendTankIndicators(answer, id);
        break;
    case SUNCSync::RequestType::SEND_FLOWERS_OUTPUT_INDICATOR:
        parseSendFlowmeterOutputIndicators(answer, id);
        break;
    case SUNCSync::RequestType::SEND_FLOWERS_INPUT_INDICATOR:
        parseSendFlowmeterInputIndicators(answer, id);
        break;
    case SUNCSync::RequestType::UNDEFINED:
    default:
        Q_ASSERT(false);
    }

    _requests.erase(it_requests);
}

void SUNCSync::errorOccurredHTTP(QNetworkReply::NetworkError code, quint64 serverCode, const QString& msg, quint64 id)
{
    const auto it_requests = _requests.constFind(id);

    Q_ASSERT(it_requests != _requests.end());

    const auto errorMsg = QString("SUNC API HTTPS: %1").arg(msg);

    emit errorRequest(msg, PackageProcessingStatus::HTTP_ERROR, id);

    _requests.erase(it_requests);
}

void SUNCSync::sendLogMsgHTTP(Common::TDBLoger::MSG_CODE category, const QString &msg, quint64 id)
{
    const auto it_requests = _requests.constFind(id);

    Q_ASSERT(it_requests != _requests.end());

    const auto logMsg = QString("SUNC API HTTPS request message: %1").arg(msg);

    emit sendLogMsg(category, logMsg, id);
}

void SUNCSync::parseGetPackageStatus(const QByteArray& answerData, quint64 id)
{
    try
    {
        QJsonParseError error;
        const auto doc = QJsonDocument::fromJson(answerData, &error);

        if (doc.isNull())
        {
            throw ParseException(QString("Answer is not JSON. Message: %1").arg(error.errorString()));
        }

        if (!doc.isObject())
        {
            throw ParseException(QString("Answer is not JSON object"));
        }

        const auto data = doc.object();

        PackageStatusInfo tmp;
        tmp.success = JSONReadBool(data, "success");
        if (!tmp.success)
        {
            tmp.error = JSONReadString(data, "error");
        }
        else
        {
            tmp.packageProcessingStatus = stringToPackageProcessingStatus(JSONReadString(data, "packageProcessingStatus", false));
        };

        emit getPackageStatus(tmp, id);
    }
    catch (const ParseException& err)
    {
        emit errorRequest(QString("GetPackageStatus: Error parsing answer from server. Error: %1. Data: %2").arg(err.what()).arg(answerData), PackageProcessingStatus::HTTP_ERROR, id);
    }
}

void SUNCSync::parseGetApplicantData(const QByteArray &answerData, quint64 id)
{
/*
{
  "success": true,
  "error": null,
  "applicantData": {
    "applicantId": 264924632537000000,
    "objects": [
      {
        "objectId": 264924640965000000,
        "objectType": "PetrolStation",
        "tanks": [
           264938628484000000
        ],
        "devices": [
          {
            "deviceId": 264938643993000000,
            "deviceType": "FlowmeterInputAvto"
          },
          {
            "deviceId": 264938647660000000,
            "deviceType": "FlowmeterOutputAvto"
          }
        ]
      }
    ]
  }
}
*/
    try
    {
        QJsonParseError error;
        const auto doc = QJsonDocument::fromJson(answerData, &error);

        if (doc.isNull())
        {
            throw ParseException(QString("Answer is not JSON. Message: %1").arg(error.errorString()));
        }

        if (!doc.isObject())
        {
            throw ParseException(QString("Answer is not JSON object"));
        }

        const auto data = doc.object();

        ApplicantDataInfo tmp;
        tmp.success = JSONReadBool(data, "success");
        if (!tmp.success)
        {
            tmp.error = JSONReadString(data, "error");

            emit getApplicantData(tmp, id);

            return;
        }

        tmp.applicantData.applicantId = static_cast<qint64>(JSONReadNumber(data, "applicantId"));
        tmp.applicantData.tokenNonce = JSONReadString(data, "tokenNonce");

        if (!data["objects"].isArray())
        {
            throw ParseException(QString("Key (Root/objects) is not JSON array"));
        }
        const auto objects = data["objects"].toArray();
        for (const auto objectRef: objects)
        {
            if (!objectRef.isObject())
            {
                throw ParseException(QString("Key (objects/object) is not JSON object"));
            }
            const auto object = objectRef.toObject();

            Object objData;
            objData.objectId = static_cast<qint64>(JSONReadNumber(object, "objectId"));
            objData.objectType = stringToObjectType(JSONReadString(object, "objectType", false));
            objData.objectName = JSONReadString(object, "objectName", false);

            if (!object["tanks"].isArray())
            {
                throw ParseException(QString("Key (objects/object/tanks) is not JSON array"));
            }

            const auto tanks = object["tanks"].toArray();
            for (const auto tankRef: tanks)
            {
                if (!tankRef.isObject())
                {
                    throw ParseException(QString("Key (objects/object/tanks/tank) is not JSON object"));
                }
                const auto tank = tankRef.toObject();

                Tank tankData;
                tankData.tankId = static_cast<qint64>(JSONReadNumber(tank, "tankId"));
                tankData.tankName = JSONReadString(tank, "tankName", false);

                objData.tanks.emplaceBack(std::move(tankData));
            }

            if (!object["devices"].isArray())
            {
                throw ParseException(QString("Key (objects/object/devices) is not JSON array"));
            }

            const auto devices = object["devices"].toArray();
            for (const auto deviceRef: devices)
            {
                if (!deviceRef.isObject())
                {
                    throw ParseException(QString("Key (objects/object/devices/device) is not JSON object"));
                }
                const auto device = deviceRef.toObject();

                Device deviceData;
                deviceData.deviceId = static_cast<qint64>(JSONReadNumber(device, "deviceId"));
                deviceData.deviceName = JSONReadString(device, "deviceName", false);
                deviceData.deviceType = stringToDeviceType(JSONReadString(device, "deviceType", false));

                objData.devices.emplaceBack(std::move(deviceData));
            }

            tmp.applicantData.objects.emplaceBack(std::move(objData));
        }

        emit getApplicantData(tmp, id);
    }
    catch (const ParseException& err)
    {
        emit errorRequest(QString("GetApplicantData: Error parsing answer from server. Error: %1. Data: %2").arg(err.what()).arg(answerData.toBase64()), PackageProcessingStatus::HTTP_ERROR, id);
    }
}

void SUNCSync::parseSendTankIndicators(const QByteArray &answerData, quint64 id)
{
/*
{
  "success": true,
  "error": null
}
*/
    try
    {
        QJsonParseError error;
        const auto doc = QJsonDocument::fromJson(answerData, &error);

        if (doc.isNull())
        {
            throw ParseException(QString("Answer is not JSON. Message: %1").arg(error.errorString()));
        }

        if (!doc.isObject())
        {
            throw ParseException(QString("Answer is not JSON object"));
        }

        const auto data = doc.object();

        TankIndicatorsInfo tmp;
        tmp.success = JSONReadBool(data, "success");
        if (!tmp.success)
        {
            tmp.error = JSONReadString(data, "error");
        }

        emit sendTankIndicators(tmp, id);
    }
    catch (const ParseException& err)
    {
        emit errorRequest(QString("SendTankIndicators: Error parsing answer from server. Error: %1. Data: %2").arg(err.what()).arg(answerData.toBase64()), PackageProcessingStatus::HTTP_ERROR, id);
    }
}

void SUNCSync::parseSendTankTransfers(const QByteArray &answerData, quint64 id)
{
/*
{
  "success": true,
  "error": null
}
*/

    try
    {
        QJsonParseError error;
        const auto doc = QJsonDocument::fromJson(answerData, &error);

        if (doc.isNull())
        {
            throw ParseException(QString("Answer is not JSON. Message: %1").arg(error.errorString()));
        }

        if (!doc.isObject())
        {
            throw ParseException(QString("Answer is not JSON object"));
        }

        const auto data = doc.object();

        TankTransfersInfo tmp;
        tmp.success = JSONReadBool(data, "success");
        if (!tmp.success)
        {
            tmp.error = JSONReadString(data, "error");
        }

        emit sendTankTransfers(tmp, id);
    }
    catch (const ParseException& err)
    {
        emit errorRequest(QString("SendTankTransfers: Error parsing answer from server. Error: %1. Data: %2").arg(err.what()).arg(answerData.toBase64()), PackageProcessingStatus::HTTP_ERROR, id);
    }
}

void SUNCSync::parseSendFlowmeterOutputIndicators(const QByteArray &answerData, quint64 id)
{
/*
{
  "success": true,
  "error": null
}
*/

    try
    {
        QJsonParseError error;
        const auto doc = QJsonDocument::fromJson(answerData, &error);

        if (doc.isNull())
        {
            throw ParseException(QString("Answer is not JSON. Message: %1").arg(error.errorString()));
        }

        if (!doc.isObject())
        {
            throw ParseException(QString("Answer is not JSON object"));
        }

        const auto data = doc.object();

        FlowmeterOutputIndicatorsInfo tmp;
        tmp.success = JSONReadBool(data, "success");
        if (!tmp.success)
        {
            tmp.error = JSONReadString(data, "error");
        }

        emit sendFlowmeterOutputIndicators(tmp, id);
    }
    catch (const ParseException& err)
    {
        emit errorRequest(QString("SendFlowmeterOutputIndicators: Error parsing answer from server. Error: %1. Data: %2").arg(err.what()).arg(answerData.toBase64()), PackageProcessingStatus::HTTP_ERROR, id);
    }
}

void SUNCSync::parseSendFlowmeterInputIndicators(const QByteArray &answerData, quint64 id)
{
/*
{
  "success": true,
  "error": null
}
*/
    try
    {
        QJsonParseError error;
        const auto doc = QJsonDocument::fromJson(answerData, &error);

        if (doc.isNull())
        {
            throw ParseException(QString("Answer is not JSON. Message: %1").arg(error.errorString()));
        }

        if (!doc.isObject())
        {
            throw ParseException(QString("Answer is not JSON object"));
        }

        const auto data = doc.object();

        FlowmeterInputIndicatorsInfo tmp;
        tmp.success = JSONReadBool(data, "success");
        if (!tmp.success)
        {
            tmp.error = JSONReadString(data, "error");
        }

        emit sendFlowmeterInputIndicators(tmp, id);
    }
    catch (const ParseException& err)
    {
        emit errorRequest(QString("SendFlowmeterInputIndicators: Error parsing answer from server. Error: %1. Data: %2").arg(err.what()).arg(answerData.toBase64()), PackageProcessingStatus::HTTP_ERROR, id);
    }
}

bool SUNCSync::Measument::check() const
{
    return measurementDate.isValid() && measurementDate <= QDateTime::currentDateTime() && measurementDate >= QDateTime::fromString("2000-01-01 00:00:00:001", DATETIME_FORMAT) &&
        mass >= 0.0f && massUnitType != MassUnitType::UNDEFINED &&
        volume >= 0.0f && volumeUnitType != VolumeUnitType::UNDEFINED &&
        level >= 0.0f  && levelUnitType != LevelUnitType::UNDEFINED &&
        density >= 0.0f && temperature >= -200.0f &&
        oilProductType != OilProductType::UNDEFINED;
}

QString SUNCSync::Measument::toString() const
{
    QString res;

    QTextStream ss(&res);
    ss << "Date: " << measurementDate.toString(DATETIME_FORMAT);
    ss << " mass: " << mass << " " << massUnitTypeToString(massUnitType);
    ss << " volume: " << volume << " " << volumeUnitTypeToString(volumeUnitType);
    ss << " level: " << level << " " <<  levelUnitTypeToString(levelUnitType);
    ss << " density: " << density;
    ss << " temperature: " << temperature;
    ss << " oil product type: " << oilProductTypeToString(oilProductType);

    return res;
}

bool SUNCSync::FlowmeterOutputMeasurementData::check() const
{
    return measurementDate.isValid() && measurementDate <= QDateTime::currentDateTime() && measurementDate >= QDateTime::fromString("2000-01-01 00:00:00:001", DATETIME_FORMAT) &&
        totalMass >= 0.0f && flowMass >= 0.0f && totalVolume >= 0.0f && currentDensity >= 0.0f && currentTemperature >= -200.0f &&
        oilProductType != OilProductType::UNDEFINED;
}

QString SUNCSync::FlowmeterOutputMeasurementData::toString() const
{
    QString res;

    QTextStream ss(&res);
    ss << "Date: " << measurementDate.toString(DATETIME_FORMAT);
    ss << " total mass: " << totalMass;
    ss << " flow mass: " << flowMass;
    ss << " total volume: " << totalVolume;
    ss << " current density: " << currentDensity;
    ss << " current temperature: " << currentTemperature;
    ss << " oil product type: " << oilProductTypeToString(oilProductType);

    return res;
}

bool SUNCSync::FlowmeterInputMeasurementData::check() const
{
    return measurementDate.isValid() && measurementDate <= QDateTime::currentDateTime() && measurementDate >= QDateTime::fromString("2000-01-01 00:00:00:001", DATETIME_FORMAT) &&
        totalMass >= 0.0f && flowMass >= 0.0f && totalVolume >= 0.0f && currentDensity >= 0.0f && currentTemperature >= -200.0f &&
        oilProductType != OilProductType::UNDEFINED;
}

QString SUNCSync::FlowmeterInputMeasurementData::toString() const
{
    QString res;

    QTextStream ss(&res);
    ss << "Date: " << measurementDate.toString(DATETIME_FORMAT);
    ss << " total mass: " << totalMass;
    ss << " flow mass: " << flowMass;
    ss << " total volume: " << totalVolume;
    ss << " current density: " << currentDensity;
    ss << " current temperature: " << currentTemperature;
    ss << " oil product type: " << oilProductTypeToString(oilProductType);

    return res;
}

bool SUNCSync::Transfer::check() const
{
    return startDate.isValid() && startDate <= QDateTime::currentDateTime() && startDate >= QDateTime::fromString("2000-01-01 00:00:00:001", DATETIME_FORMAT) &&
           endDate.isValid() && endDate <= QDateTime::currentDateTime() && endDate >= QDateTime::fromString("2000-01-01 00:00:00:001", DATETIME_FORMAT) &&
           massStart >= 0.0f && massEnd >= 0.0f &&massUnitType != MassUnitType::UNDEFINED &&
           volumeStart >= 0.0f && volumeEnd >= 0.0f && volumeUnitType != VolumeUnitType::UNDEFINED &&
           levelStart >= 0.0f  && levelEnd >= 0.0f  && levelUnitType != LevelUnitType::UNDEFINED &&
           density >= 0.0f &&
           temperature >= -200.0f &&
           oilProductType != OilProductType::UNDEFINED;
}

QString SUNCSync::Transfer::toString() const
{
    QString res;

    QTextStream ss(&res);
    ss << "Start date: " << startDate.toString(DATETIME_FORMAT);
    ss << " end date: " << endDate.toString(DATETIME_FORMAT);
    ss << " start mass: " << massStart << " " << massUnitTypeToString(massUnitType);
    ss << " end mass: " << massEnd << " " << massUnitTypeToString(massUnitType);
    ss << " start volume: " << volumeStart << " " << volumeUnitTypeToString(volumeUnitType);
    ss << " end volume: " << volumeEnd << " " << volumeUnitTypeToString(volumeUnitType);
    ss << " start level: " << levelStart << " " <<  levelUnitTypeToString(levelUnitType);
    ss << " end level: " << levelEnd << " " <<  levelUnitTypeToString(levelUnitType);
    ss << " density: " << density;
    ss << " temperature: " << temperature;
    ss << " oil product type: " << oilProductTypeToString(oilProductType);

    return res;
}
