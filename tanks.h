#pragma once

//STL
#include <memory>
#include <unordered_map>
#include <string>

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
    Tanks(const Common::DBConnectionInfo dbConnectionInfo, TanksConfig* tanksConfig, QObject* parent = nullptr);
    ~Tanks();

public slots:
    void start();
    void stop();

private slots:
    void calculateStatusesTank(const LevelGaugeService::TankID& id, const TankStatusesList &tankStatuses);
    void calculateIntakesTank(const LevelGaugeService::TankID& id, const IntakesList &intakes);

    void errorOccurredTank(const LevelGaugeService::TankID& id, Common::EXIT_CODE errorCode, const QString& msg);
    void sendLogMsgTank(const LevelGaugeService::TankID& id, Common::TDBLoger::MSG_CODE category, const QString &msg);

    void loadFromMeasumentsDB();  //загружает новые данные из таблицы измерений
    void startedTank(const LevelGaugeService::TankID& id);

signals:
    void stopAll();
    void errorOccurred(Common::EXIT_CODE errorCode, const QString& errorString);
    void sendLogMsg(Common::TDBLoger::MSG_CODE category, const QString &msg);
    void finished();

    void newStatuses(const LevelGaugeService::TankID& id, const LevelGaugeService::TankStatusesList& tankStatuses);

    void calculateStatuses(const LevelGaugeService::TankID& id, const TankStatusesList &tankStatuses);
    void calculateIntakes(const LevelGaugeService::TankID& id, const IntakesList &intakes);

private:  
    using TanksLoadStatuses = std::unordered_map<TankID, TankStatusesList>;

private:
    Tanks() = delete;
    Q_DISABLE_COPY_MOVE(Tanks)

    TanksLoadStatuses loadFromCalculatedDB();  //загружает данне о предыдыщих сохранениях из БД

    void makeTanks();

    QString tanksFilterCalculate() const;
    QString tanksFilterMeasument() const;

private:
    struct TankThread
    {
        std::unique_ptr<Tank> tank;
        std::unique_ptr<QThread> thread;
        bool isStarted = false;
    };

private:
    const Common::DBConnectionInfo _dbConnectionInfo;
    LevelGaugeService::TanksConfig* _tanksConfig = nullptr;

    QTimer* _checkNewMeasumentsTimer = nullptr;

    QSqlDatabase _db;

    std::unordered_map<LevelGaugeService::TankID, std::unique_ptr<TankThread>> _tanks;

    quint64 _lastLoadId = 0;

    bool _isFirstLoadMeasuments = true;

    bool _isStarted = false;
};

} //namespace LevelGaugeService

template<>
struct std::hash<QUuid>
{
    std::size_t operator()(const QUuid& id) const noexcept
    {
        return std::hash<std::string>{}(id.toString().toStdString());
    }
};

