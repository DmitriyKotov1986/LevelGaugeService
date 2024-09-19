///////////////////////////////////////////////////////////////////////////////
/// Класс выполняет синхронизацию c БД
///
/// (с) Dmitriy Kotov, 2024
///////////////////////////////////////////////////////////////////////////////
#pragma once

//STL
#include <queue>

//Qt
#include <QObject>
#include <QHash>
#include <QTimer>
#include <QQueue>
#include <QUuid>

//My
#include "Common/common.h"
#include "tankstatuses.h"
#include "tanksconfig.h"
#include "sync.h"

namespace LevelGaugeService
{

///////////////////////////////////////////////////////////////////////////////
/// Класс выполняет сихронизацию с БД
///
class SyncDBStatus final
    : public SyncImpl
{
    Q_OBJECT

public:
    /*!
        Конструктор
        @param dbConnectionInfo - ссылка на информацию о подключении к БД
        @param tankConfig - ссылка на конфигурации резервуаров
        @param parent - указатель на родительский класс
    */
    SyncDBStatus(const Common::DBConnectionInfo& dbConnectionInfo, LevelGaugeService::TanksConfig* tanksConfig, QObject* parent = nullptr);

    /*!
        Деструктор
    */
    ~SyncDBStatus();

    void calculateStatuses(const LevelGaugeService::TankID& id, const LevelGaugeService::TankStatusesList& tankStatuses) override;

    void start() override;
    void stop() override;

private:
     SyncDBStatus() = delete;
     Q_DISABLE_COPY_MOVE(SyncDBStatus)

private slots:
     void saveToDB();

private:
    TanksConfig* _tanksConfig;
    const Common::DBConnectionInfo _dbConnectionInfo;

    QSqlDatabase _db;      //база данных с исходными данными

    bool _isStarted = false;

    QTimer* _saveTimer = nullptr;

    QHash<LevelGaugeService::TankID, LevelGaugeService::TankStatusesList> _dataForSave;

}; //class Sync

} //namespace LevelGaugeService


