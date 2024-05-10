//*****************************************************************************
//
// Класс выполняет синхронизацию сгенерированной таблице показаний с
// БД АО НИТ
//
//*****************************************************************************
#ifndef SYNC_H
#define SYNC_H

//STL
#include <queue>

//Qt
#include <QObject>
#include <QHash>
#include <QTimer>
#include <QQueue>

//My
#include "tconfig.h"
#include "tankstatuses.h"
#include "tanksconfig.h"
#include "intake.h"

namespace LevelGaugeService
{

class Sync final
    : public QObject
{
    Q_OBJECT

public:
    Sync() = delete;

    Sync(const Sync&) = delete;
    Sync& operator =(const Sync&) = delete;
    Sync(const Sync&&) = delete;
    Sync& operator =(const Sync&&) = delete;

    Sync(const Common::DBConnectionInfo& dbConnectionInfo, const Common::DBConnectionInfo& dbNitConnectionInfo,
         LevelGaugeService::TanksConfig* tanksConfig, QObject *parent = nullptr);

    ~Sync();

public slots:
    void newStatuses(const LevelGaugeService::TankID& id, const LevelGaugeService::TankStatusesList& tankStatuses);
    void newIntakes(const LevelGaugeService::TankID& id, const LevelGaugeService::IntakesList& intakes);

    void start();
    void stop();

signals:
    void errorOccurred(Common::EXIT_CODE errorCode, const QString& errorString);
    void finished() const;

private:
    void saveToDB(const LevelGaugeService::TankID& id, const LevelGaugeService::TankStatus& tankStatus, quint64 recordID);
    void saveToDBNitOilDepot(const LevelGaugeService::TankID& id, const LevelGaugeService::TankStatus& tankStatus);
    void saveToDBNitAZS(const LevelGaugeService::TankID& id, const LevelGaugeService::TankStatus& tankStatus);

    quint64 getLastSavetId(const LevelGaugeService::TankID& id);

    void saveIntakesToNIT(const LevelGaugeService::TankID& id, const LevelGaugeService::IntakesList& intakes);
    void saveIntakes(const LevelGaugeService::TankID& id, const LevelGaugeService::IntakesList& intakes);

    void executeDBQuery(QSqlDatabase& db, const QString& queryText);

private:
    TanksConfig* _tanksConfig = nullptr;
    const Common::DBConnectionInfo _dbConnectionInfo;
    const Common::DBConnectionInfo _dbNitConnectionInfo;

    QSqlDatabase _db;      //база данных с исходными данными
    QSqlDatabase _dbNit;   //база данных АО НИТ

}; //class Sync

} //namespace LevelGaugeService

#endif // SYNC_H
