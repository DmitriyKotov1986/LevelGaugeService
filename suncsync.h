#pragma once

//Qt
#include <QObject>
#include <QUuid>
#include <QUrl>

//My
#include "Common/httpsslquery.h"

namespace LevelGaugeService
{

class SUNCSync final
     : public QObject
{
    Q_OBJECT

public:
    ///< Типы запроса. Необходима для вызова нужного парсера
    enum class RequestType: quint8
    {
        UNDEFINED = 0,                     ///< Неопределено
        GET_PACKAGE_STATUS = 1,            ///< Получения статуса ранее отправленнного пакета
        GET_APPLICANT_DATA = 2,            ///< Получение данных о огранизации
        SEND_TANK_INDICATORS = 3,          ///< Передача статусов резервуаров
        SEND_TANK_TRANSFER = 4,            ///< Передача данных о перемещении топлиав
        SEND_FLOWERS_OUTPUT_INDICATOR = 5,
        SEND_FLOWERS_INPUT_INDICATOR = 6
    };

    //PackageStatus
    enum class PackageProcessingStatus: quint8
    {
        UNDEFINED = 0,              ///< Неопределено
        SEND_TO_SERVER = 1,         ///< Отправлен запрос на сервер
        PENDING = 2,                ///< Пакет на сервер доставлен и принят к обработке
        INCORRECT_DATA_ERROR = 3,   ///< Ошибка отправки - пакет содержит некорректные данные
        HTTP_ERROR = 4,             ///< Ошибка отправки - ошибка при пересылке данных. Необходимо повторить отправку
        SERVER_ERROR = 5,           ///<  Ошибка отправки - сервер не принял данные
        SUCCESS = 10                ///<  Сервер успешно принял данные и обработал их
    };

    struct PackageStatusInfo
    {
        bool success = false;
        QString error;
        PackageProcessingStatus packageProcessingStatus = PackageProcessingStatus::UNDEFINED;
    };

    //ApplicantData
    struct Tank
    {
        qint64 tankId = 0;
        QString tankName;
    };

    enum class DeviceType: quint8
    {
        UNDEFINED = 0,
        FLOWMETER_OUTPUT_AVTO = 1,
        FLOWMETER_OUTPUT_ZHD = 2,
        FLOWMETER_OUTPUT_TUBE = 3,
        FLOWMETER_INPUT_AVTO = 4,
        FLOWMETER_INPUT_ZHD = 5,
        FLOWMETER_INPUT_TUBE = 6,
        ELECTRONIC_SCALE = 7,
        TRANSFER = 8
    };

    struct Device
    {
        qint64 deviceId = 0;
        QString deviceName;
        DeviceType deviceType = DeviceType::UNDEFINED;
    };

    enum class ObjectType: quint8
    {
        UNDEFINED = 0,
        PETROL_STATION = 1,
        REFINERY = 2,
        REFINERY_LOW = 3,
        OIL_BASE = 4
    };

    struct Object
    {
        qint64 objectId = 0;
        ObjectType objectType = ObjectType::UNDEFINED;
        QString objectName;
        QList<Tank> tanks;
        QList<Device> devices;
    };

    struct ApplicantData
    {
        qint64 applicantId = 0;
        QList<Object> objects;
        QString tokenNonce;
    };

    struct ApplicantDataInfo
    {
        bool success = false;
        QString error;
        ApplicantData applicantData;
    };

//SendTankIndicators
    enum class OilProductType: quint16
    {
        UNDEFINED = 0,
        AI76 = 76,
        AI80 = 80,
        AI91 = 91,
        AI93 = 93,
        AI92 = 92,
        AI92K4 = 492,
        AI92K5 = 592,
        AI92PRIME = 692,
        AI95 = 95,
        AI95K4 = 495,
        AI95K5 = 595,
        AI95PREMIUM = 795,
        AI95PRIME = 692,
        G95 = 895,
        AI96 = 96,
        AI96K4 = 496,
        AI98 = 98,
        AI98K4 = 498,
        AI98K5 = 598,
        AI98SUPER = 998,
        AI98PRIME= 698,
        AI100 = 100,
        G100= 8100,
        DT = 10,
        DTZ = 11,
        DTZK2 = 211,
        DTZK4 = 411,
        DTZK5 = 511,
        DTZPRIME = 611,
        DTL = 12,
        DTLK4 = 412,
        DTLK5 = 512,
        DTLPRIME = 612,
        DTA = 13,
        DTAK2 = 213,
        DTAK4 = 413,
        DTAK5 = 513,
        DTE = 14,
        DTEK2 = 214,
        DTEK4 = 414,
        DTEK5 = 514,
        DTXP = 15,
        DTM =16,
        M100 = 9100,
        ZM40 = 1040,
        ZM100 = 10100,
        TS1 = 1,
        JETFUEL = 2,
        PTB = 3,
        HYDRAZINE =4,
        NPD = 5,
        DISTILLYAT = 6,
        UNKNOWN = 65534,
        NEFRAS = 7
    };

    enum class MassUnitType: quint64
    {
        UNDEFINED = 0,
        KILOGRAM = 1,
        QUINTAL = 2,
        TON = 1000
    };

    enum class VolumeUnitType: quint64
    {
        UNDEFINED = 0,
        CUBIC_CENTIMETER = 1,
        CUBIC_DECIMETER = 1000,
        CUBIC_METER = 1000000
    };

    enum class LevelUnitType: quint64
    {
        UNDEFINED = 0,
        MILLIMETER = 1,
        CENTIMETER = 10,
        METER = 1000
    };

    struct Measument
    {
        QDateTime measurementDate;
        float mass = 0.0f;
        MassUnitType massUnitType = MassUnitType::UNDEFINED;
        float volume = 0.0f;
        VolumeUnitType volumeUnitType = VolumeUnitType::UNDEFINED;
        float level = 0.0f;
        LevelUnitType levelUnitType = LevelUnitType::UNDEFINED;
        float density = 0.0f;
        float temperature = 0.0f;
        OilProductType oilProductType = OilProductType::UNDEFINED;

        bool check() const;
        QString toString() const;
    };

    struct TankMeasurements
    {
        qint64 tankId;
        QList<Measument> measuments;
    };

    struct TankIndicators
    {
        QUuid packageId;
        QList<TankMeasurements> tankMeasurements;

    };

    struct TankIndicatorsInfo
    {
        bool success = false;
        QString error;
    };


    //SendTankTransfers
    struct Transfer
    {
        QDateTime startDate;
        QDateTime endDate;
        float massStart = 0.0f;
        float massEnd = 0.0f;
        MassUnitType massUnitType = MassUnitType::UNDEFINED;
        float volumeStart = 0.0f;
        float volumeEnd = 0.0f;
        VolumeUnitType volumeUnitType = VolumeUnitType::UNDEFINED;
        float levelStart = 0.0f;
        float levelEnd = 0.0f;
        LevelUnitType levelUnitType = LevelUnitType::UNDEFINED;
        float density = 0.0f;
        float temperature = 0.0f;
        OilProductType oilProductType = OilProductType::UNDEFINED;  

        bool check() const;
        QString toString() const;
    };

    enum class TransferOperationType: quint8
    {
        UNDEFINED = 0,
        INCOME = 1,
        OUTCOME = 2
    };

    struct TankTransfers
    {
        qint64 tankId;
        TransferOperationType operationType = TransferOperationType::UNDEFINED;
        QList<Transfer> transfers;
    };
    
    struct TanksTransfers
    {
        QUuid packageId;
        QList<TankTransfers> tankTransfers;
    };
    
    struct TankTransfersInfo
    {
        bool success = false;
        QString error;        
    };

    //SendFlowmeterOutputIndicators
    struct FlowmeterOutputMeasurementData
    {
        QDateTime measurementDate;
        float totalMass = 0.0f;
        float flowMass = 0.0f;
        float totalVolume = 0.0f;
        float currentDensity = 0.0f;
        float currentTemperature = 0.0f;
        OilProductType oilProductType = OilProductType::UNDEFINED;

        bool check() const;
        QString toString() const;
    };

    struct FlowmeterOutputMeasurements
    {
        qint64 deviceId = 0;
        QList<FlowmeterOutputMeasurementData> flowmeterOutputMeasurementData;
    };

    struct FlowmeterOutputIndicators
    {
        QUuid packageId;
        QList<FlowmeterOutputMeasurements> flowmeterOutputMeasurements;
    };

    struct  FlowmeterOutputIndicatorsInfo
    {
        bool success = false;
        QString error;
    };

    //SendFlowmeterInputIndicators
    struct FlowmeterInputMeasurementData
    {
        QDateTime measurementDate;
        float totalMass = 0.0f;
        float flowMass = 0.0f;
        float totalVolume = 0.0f;
        float currentDensity = 0.0f;
        float currentTemperature = 0.0f;
        OilProductType oilProductType = OilProductType::UNDEFINED;

        bool check() const;
        QString toString() const;
    };

    struct FlowmeterInputMeasurements
    {
        qint64 deviceId = 0;
        QList<FlowmeterInputMeasurementData> flowmeterInputMeasurementData;
    };

    struct FlowmeterInputIndicators
    {
        QUuid packageId;
        QList<FlowmeterInputMeasurements> flowmeterInputMeasurements;
    };

    struct  FlowmeterInputIndicatorsInfo
    {
        bool success = false;
        QString error;
    };

public:
    static QString requestGuid();

    static PackageProcessingStatus stringToPackageProcessingStatus(const QString& status);
    static QString packageProcessingStatusToString(PackageProcessingStatus status);
    static DeviceType stringToDiviceType(const QString& type);
    static ObjectType stringToObjectType(const QString& type);
    static QString massUnitTypeToString(MassUnitType type);
    static QString volumeUnitTypeToString(VolumeUnitType type);
    static QString levelUnitTypeToString(LevelUnitType type);
    static QString transferOperationTypeToString(TransferOperationType type);

    static QString oilProductTypeToString(OilProductType type);
    static OilProductType stringToOilProductType(const QString& type);

public:
    /*!
        Конструктор
        @param parent - указатель на родительский класс
    */
    explicit SUNCSync(const QUrl& baseUrl, const QString& remoteBearerToken, QObject* parent = nullptr);

    /*!
        Деструктор
    */
    ~SUNCSync();

    /*!
        Отправляет список новых статусов резервуара. В случае ошибки возвращает 0 и генерируеться сигнал errorRequest(...)
        @param packageID - ИД пакета
        @return - ИД запроса или 0 если данные некорректны или произошла ошибка отправки
     */
    quint64 sendGetPackageStatus(const QUuid& packageID);
    quint64 sendGetApplicantData(const QString& bin);

    /*!
        Отправляет список новых статусов резервуара. В случае ошибки возвращает 0 и генерируеться сигнал errorRequest(...)
        @param tankIndicators - данные для отправки
        @return - ИД запроса или 0 если данные некорректны или произошла ошибка отправки
     */
    quint64 sendSendTankIndicators(const TankIndicators& tankIndicators);
    quint64 sendSendTankTransfers(const TanksTransfers& tanksTransfers);
    quint64 sendSendFlowmeterOutputIndicators(const FlowmeterOutputIndicators& flowmeterOutputIndicators);
    quint64 sendSendFlowmeterInputIndicators(const FlowmeterInputIndicators& flowmeterInputIndicators);

signals:
    /*!
        Испускаеться при ошибке обработки запроса
        @param msg - текстовое описание ошибки
        @param id - ИД запроса
    */
    void errorRequest(const QString& msg, LevelGaugeService::SUNCSync::PackageProcessingStatus status, quint64 id);
    void sendLogMsg(Common::TDBLoger::MSG_CODE category, const QString& msg, quint64 id);

    void getPackageStatus(const LevelGaugeService::SUNCSync::PackageStatusInfo& status, quint64 id);
    void getApplicantData(const LevelGaugeService::SUNCSync::ApplicantDataInfo& applicantData, quint64 id);
    void sendTankIndicators(const LevelGaugeService::SUNCSync::TankIndicatorsInfo& tankIndicators, quint64 id);
    void sendTankTransfers(const LevelGaugeService::SUNCSync::TankTransfersInfo& tankTransfers, quint64 id);
    void sendFlowmeterOutputIndicators(const LevelGaugeService::SUNCSync::FlowmeterOutputIndicatorsInfo& tankTransfers, quint64 id);
    void sendFlowmeterInputIndicators(const LevelGaugeService::SUNCSync::FlowmeterInputIndicatorsInfo& tankTransfers, quint64 id);

private slots:
    void getAnswerHTTP(const QByteArray& answer, quint64 id);
    void errorOccurredHTTP(QNetworkReply::NetworkError code, quint64 serverCode, const QString& msg, quint64 id);
    void sendLogMsgHTTP(Common::TDBLoger::MSG_CODE category, const QString& msg, quint64 id);

private:
    // удаляем неиспользуемые конструкторы
    Q_DISABLE_COPY_MOVE(SUNCSync)

    void parseGetPackageStatus(const QByteArray& answerData, quint64 id);
    void parseGetApplicantData(const QByteArray& answerData, quint64 id);
    void parseSendTankIndicators(const QByteArray& answerData, quint64 id);
    void parseSendTankTransfers(const QByteArray& answerData, quint64 id);
    void parseSendFlowmeterOutputIndicators(const QByteArray& answerData, quint64 id);
    void parseSendFlowmeterInputIndicators(const QByteArray& answerData, quint64 id);

private:
    Common::HTTPSSLQuery _query;
    Common::HTTPSSLQuery::Headers _headers;

    QHash<quint64, RequestType> _requests;

    const QString _remoteBearerToken;

    const QUrl _baseUrl;

};  //SUNCSync

} // namespace LevelGaugeService

Q_DECLARE_METATYPE(LevelGaugeService::SUNCSync::PackageStatusInfo)
Q_DECLARE_METATYPE(LevelGaugeService::SUNCSync::ApplicantDataInfo)
Q_DECLARE_METATYPE(LevelGaugeService::SUNCSync::TankIndicatorsInfo)
Q_DECLARE_METATYPE(LevelGaugeService::SUNCSync::TankTransfersInfo)
Q_DECLARE_METATYPE(LevelGaugeService::SUNCSync::FlowmeterOutputIndicatorsInfo)
Q_DECLARE_METATYPE(LevelGaugeService::SUNCSync::FlowmeterInputIndicatorsInfo)
