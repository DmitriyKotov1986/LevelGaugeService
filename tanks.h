#ifndef TANKS_H
#define TANKS_H

//STL
#include <memory>
#include <unordered_map>

//QT
#include <QObject>
#include <QTimer>
#include <QThread>

//My
#include "Common/common.h"
#include "Common/tdbloger.h"
#include "tanksconfig.h"
#include "tank.h"

namespace LevelGaugeService
{

class Tanks
    : public QObject
{
    Q_OBJECT

public:
    Tanks() = delete;
    Tanks(const Tanks&) = delete;
    Tanks& operator =(const Tanks&) = delete;
    Tanks(const Tanks&&) = delete;
    Tanks& operator =(const Tanks&&) = delete;

    Tanks(const Common::DBConnectionInfo dbConnectionInfo, TanksConfig* tanksConfig, QObject* parent = nullptr);
    ~Tanks();

public slots:
    void start();
    void stop();

private slots:
    void calculateStatuses(const LevelGaugeService::TankID& id, const LevelGaugeService::TankStatusesList& tankStatuses);
    void intake(const LevelGaugeService::TankID& id, const LevelGaugeService::IntakesList& intakes);
    void errorOccurredTank(const LevelGaugeService::TankID& id, Common::EXIT_CODE errorCode, const QString& msg);
    void sendLogMsgTank(const LevelGaugeService::TankID& id, Common::TDBLoger::MSG_CODE category, const QString &msg);

    void checkNewMeasuments();

signals:
    void stopAll();
    void errorOccurred(Common::EXIT_CODE errorCode, const QString& errorString);
    void sendLogMsg(Common::TDBLoger::MSG_CODE category, const QString &msg);
    void finished();

    void newStatuses(const LevelGaugeService::TankID& id, const LevelGaugeService::TankStatusesList& tankStatuses);
    void newIntakes(const LevelGaugeService::TankID& id, const LevelGaugeService::IntakesList& intakes);

private:
    bool loadFromCalculatedDB();  //загружает данне о предыдыщих сохранениях из БД
    bool loadFromMeasumentsDB();  //загружает новые данные из таблицы измерений
    bool makeTanks();

private:
    struct TankThread
    {
        std::unique_ptr<Tank> tank;
        std::unique_ptr<QThread> thread;
    };

private:
    const Common::DBConnectionInfo _dbConnectionInfo;
    LevelGaugeService::TanksConfig* _tanksConfig = nullptr;

    QTimer* _checkNewMeasumentsTimer = nullptr;

    QSqlDatabase _db;

    std::unordered_map<LevelGaugeService::TankID, std::unique_ptr<TankThread>> _tanks;

    quint64 _lastLoadId = 0;

};


} //namespace LevelGaugeService

#endif // TANKS_H
