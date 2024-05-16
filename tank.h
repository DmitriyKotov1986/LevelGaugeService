#ifndef TANK_H
#define TANK_H

//STL
#include <map>
#include <memory>
#include <optional>

//QT
#include <QObject>
#include <QDateTime>
#include <QSqlDatabase>
#include <QTimer>
#include <QRandomGenerator>
#include <QPair>
#include <QHash>
#include <QThread>

//My
#include "Common/common.h"
#include "Common/tdbloger.h"
#include "intake.h"
#include "tankstatuses.h"
#include "tankconfig.h"

namespace LevelGaugeService
{

class TankTest;

class Tank
    : public QObject
{
    Q_OBJECT

    friend TankTest;    

public:   
    Tank() = delete;
    Tank(const Tank&) = delete;
    Tank& operator =(const Tank&) = delete;
    Tank(const Tank&&) = delete;
    Tank& operator =(const Tank&&) = delete;

    Tank(const LevelGaugeService::TankConfig* tankConfig, TankStatusesList&& tankSavedStatuses,  QObject *parent = nullptr);
    ~Tank();

public slots:
    void start();
    void stop();

    void newStatuses(const LevelGaugeService::TankID& id, const LevelGaugeService::TankStatusesList& tankStatuses);

private slots:
    void addStatusEnd();
    void sendNewStatusesToSave();

signals:
    void calculateStatuses(const LevelGaugeService::TankID& id, const LevelGaugeService::TankStatusesList& tankStatuses);
    void calculateIntakes(const LevelGaugeService::TankID& id, const LevelGaugeService::IntakesList& intakes);
    void errorOccurred(const LevelGaugeService::TankID& id, Common::EXIT_CODE errorCode, const QString& msg) const;
    void sendLogMsg(const LevelGaugeService::TankID& id, Common::TDBLoger::MSG_CODE category, const QString &msg);
    void finished() const;

private:
    using TankStatusesIterator =  LevelGaugeService::TankStatuses::iterator;

private:
    void addStatuses(const LevelGaugeService::TankStatusesList& tankStatuses);

    void addStatus(const LevelGaugeService::TankStatus& tankStatus);
    void addStatus(LevelGaugeService::TankStatus&& tankStatus);

    void addStatusesRange(const LevelGaugeService::TankStatus& tankStatus);
    void addStatusesIntake(const LevelGaugeService::TankStatus& tankStatus);

    void addRandom(LevelGaugeService::TankStatus* tankStatus) const;
    void checkLimits(LevelGaugeService::TankStatus* tankStatus) const; //провеверяет лимитные ограничения статусов

    void clearTankStatuses();

    TankStatusesIterator getStartIntake();
    TankStatusesIterator getFinishedIntake();
    void findIntake();

    TankStatusesIterator getStartPumpingOut();
    TankStatusesIterator getFinishedPumpingOut();
    void findPumpingOut();

private:
    const LevelGaugeService::TankConfig* _tankConfig = nullptr; //Конфигурация резервуар

    QRandomGenerator* _rg = nullptr;  //генератор случайных чисел для имитации разброса параметров при измерении в случае подстановки

    LevelGaugeService::TankStatuses _tankStatuses;
    QDateTime _lastSendToSaveDateTime;
    QDateTime _lastPumpingOut;

    std::optional<QDateTime> _isIntake;
    std::optional<QDateTime> _isPumpingOut;

    QTimer* _addEndTimer = nullptr;
    QTimer* _saveToDBTimer = nullptr;

};

/*





private slots:
    void calculate();

signals:
    void addStatusForSync(const LevelGaugeService::TankID& id,const QDateTime dateTime, const TankStatuses::TankStatus& status) const;
    void errorOccurred(const QString& msg);
    void finished();

private:
    void dbCommit();
    void errorDBQuery(const QSqlQuery& query);
    void dbQueryExecute(QSqlQuery &query, const QString &queryText);

    void loadTankConfig();

    void initFromSave();
    void loadFromMeasument();
    void makeResultStatus();         //
    void statusDetect();
 //   void checkLimits(TankStatuses::TankStatusesIterator start_it);         //провеверяет лимитные ограничения статусов
    void addStatusRange(const QDateTime& targetDateTime, const TankStatuses::TankStatus& targetStatus);
    void addStatusIntake(const TankStatuses::TankStatus& targetStatus);
    void addStatusEnd();
 //   void addRandom(TankStatuses::TankStatusesIterator it);
    void sendNewStatuses();


private:
    const TankID _id;

    TankConfig _tankConfig; //Конфигурация резервуар
    TankStatuses _tankResultStatuses; //карта результирующих состояний
    TankStatuses _tankTargetStatuses; //карта целевых состояний

    QSqlDatabase _db;
    const Common::DBConnectionInfo _dbConnectionInfo;
    const QString _dbConnectionName;

    QTimer* _timer = nullptr;


};
*/
} //namespace LevelGaugeService

#endif // TANK_H
