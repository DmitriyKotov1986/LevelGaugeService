#pragma once

//STL
#include <optional>

//Qt
#include <QObject>
#include <QHash>
#include <QSet>
#include <QTimer>

//My
#include "Common/common.h"
#include "tanksconfig.h"
#include "sync.h"

#include "suncsync.h"

namespace LevelGaugeService
{

class SyncHTTPIntake final
    : public  SyncImpl
{
    Q_OBJECT

public:
    /*!
        Конструктор
        @param dbConnectionInfo - ссылка на информацию о подключении к БД
        @param tankConfig - ссылка на конфигурацию резервуара
        @param parent - указатель на родительский класс
    */
    SyncHTTPIntake(const Common::DBConnectionInfo& dbConnectionInfo, LevelGaugeService::TanksConfig* tanksConfig, QObject* parent = nullptr);

    /*!
        Деструктор
    */
    ~SyncHTTPIntake();

    void start() override;
    void stop() override;

private slots:
    void sendNewIntakes();
    void checkPackage();

    void sendLogMsgHTTP(Common::TDBLoger::MSG_CODE category, const QString& msg, quint64 id);
    void errorRequest(const QString& msg, LevelGaugeService::SUNCSync::PackageProcessingStatus status, quint64 id);
    void getPackageStatus(const LevelGaugeService::SUNCSync::PackageStatusInfo& status, quint64 id);
    void sendTankTransfers(const LevelGaugeService::SUNCSync::TankTransfersInfo& tankTransfers, quint64 id);

private:
    using IdList = QStringList;

    using LastSendDateTime = std::unordered_map<TankID, QDateTime>;

    struct CheckPackageData
    {
        QUuid packageId;
        LevelGaugeService::TankID tankId;
    };

    struct PackageInfo
    {
        QUuid packageId;
        LevelGaugeService::SUNCSync::RequestType type = SUNCSync::RequestType::UNDEFINED;
        LastSendDateTime lastSendDateTime;
    };

    struct ApplicantData
    {
        std::unique_ptr<LevelGaugeService::SUNCSync> suncSync;
        std::list<TankID> tanksID;
    };

private:
    // Удаляем неиспользуемые конструкторы
    SyncHTTPIntake() = delete;
    Q_DISABLE_COPY_MOVE(SyncHTTPIntake)

    std::optional<CheckPackageData> needCheckPackageFromDB(const QString& tableName); ///< загружает повторно все записи по которые находятся в состоянии обработки и отправлет запрос на подтверждение

    void sendNewIntakesFromDB(qint64 applicantID);
    /*!
        Проверяет состояние обработки отправленного на сервер пакета
        @param packetId - ИД пакета
    */
    void sendCheckPackage(const CheckPackageData& packageData);

    void updatePackageIntake(const IdList &idList, const QUuid& packageID, SUNCSync::PackageProcessingStatus status);
    void updatePackageIntake(const QUuid& packageId, SUNCSync::PackageProcessingStatus status, const QString& errorMessage);
    void clearPackageIntake(const QUuid& packageId);

    QString tankFilter(qint64 applicantID) const;

private:
    const Common::DBConnectionInfo _dbConnectionInfo;
    LevelGaugeService::TanksConfig* _tanksConfig;

    std::unordered_map<qint64, ApplicantData> _suncSyncs; //key - ApplicantID, value SUNCSync;

    QSqlDatabase _db;      //база данных с исходными данными

    QHash<quint64, PackageInfo> _sendedRequest; ///< Карта отправленных запросов для которух нужно проверить статус. Ключ - ИД запроса из SUNCSync

    QTimer* _checkIntakeTimer = nullptr;
    QTimer* _sendIntakeTimer = nullptr;

    bool _isStarted = false;
    quint64 _sendedIntakeCount = 0; // количество незвершенных  запросов со статусами резервуаров отправленных на сервер
    bool _isSendedCheckPackage = false;
};

}


