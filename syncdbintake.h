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
#include "intake.h"
#include "sync.h"

namespace LevelGaugeService
{

///////////////////////////////////////////////////////////////////////////////
/// Класс выполняет сихронизацию с БД
///
class SyncDBIntake final
    : public SyncImpl
{
    Q_OBJECT

public:
    /*!
        Конструктор
        @param dbConnectionInfo - ссылка на информацию о подключении к БД
        @param tankConfig - ссылка на конфигурацию резервуара
        @param parent - указатель на родительский класс
    */
    SyncDBIntake(const Common::DBConnectionInfo& dbConnectionInfo, LevelGaugeService::TanksConfig* tanksConfig, QObject* parent = nullptr);

    /*!
        Деструктор
    */
    ~SyncDBIntake();

    void calculateIntakes(const LevelGaugeService::TankID& id, const LevelGaugeService::IntakesList& intakes) override;

    void start() override;
    void stop() override;

private:
     SyncDBIntake() = delete;
     Q_DISABLE_COPY_MOVE(SyncDBIntake)

    void saveIntakesToDB(const LevelGaugeService::TankID& id, const LevelGaugeService::IntakesList& intakes);

private:
    TanksConfig* _tanksConfig;
    const Common::DBConnectionInfo _dbConnectionInfo;

    QSqlDatabase _db;      //база данных с исходными данными

    bool _isStarted = false;

}; //class Sync

} //namespace LevelGaugeService


