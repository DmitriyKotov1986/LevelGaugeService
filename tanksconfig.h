#pragma once

//STL
#include <memory>
#include <unordered_map>

//QT
#include <QObject>
#include <QDateTime>
#include <QPair>
#include <QSqlDatabase>

//My
#include "Common/common.h"
#include "Common/tdbloger.h"
#include "tankconfig.h"
#include "tankid.h"

namespace LevelGaugeService
{

class TanksConfig
    : public QObject
{
    Q_OBJECT

public:
    explicit TanksConfig(const Common::DBConnectionInfo& dbConnectionInfo, QObject* parent = nullptr);
    ~TanksConfig();

    bool loadFromDB();

    bool checkID(const LevelGaugeService::TankID& id) const;

    TankConfig* getTankConfig(const LevelGaugeService::TankID& id);

    TankIDList getTanksID() const;

    bool isExist(const LevelGaugeService::TankID& id) const;

    quint64 tanksCount() const;

signals:
    void errorOccurred(Common::EXIT_CODE errorCode, const QString& errorString);
    void sendLogMsg(Common::TDBLoger::MSG_CODE category, const QString &msg);

private slots:
    void lastMeasuments(const TankID& id, const QDateTime& lastTime);
    void lastSave(const TankID& id, const QDateTime& lastTime);
    void lastIntake(const TankID& id, const QDateTime& lastTime);
    void lastSend(const TankID& id, const QDateTime& lastTime);
    void lastSendIntake(const TankID& id, const QDateTime& lastTime);

private:
    TanksConfig() = delete;
    Q_DISABLE_COPY_MOVE(TanksConfig)

    void lastUpdate(const QString& fieldName, const TankID &id, const QDateTime &lastTime);

private:
    const Common::DBConnectionInfo _dbConnectionInfo;
    QSqlDatabase _db;

    std::unordered_map<TankID, std::unique_ptr<TankConfig>> _tanksConfig;

};

} //namespace LevelGaugeService

