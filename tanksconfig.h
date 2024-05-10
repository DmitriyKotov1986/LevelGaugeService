#ifndef TANKSCONFIG_H
#define TANKSCONFIG_H

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
    TanksConfig() = delete;
    TanksConfig(const TanksConfig&) = delete;
    TanksConfig& operator=(const TanksConfig&) = delete;
    TanksConfig(TanksConfig&&) = delete;
    TanksConfig& operator=(TanksConfig&&) = delete;

    explicit TanksConfig(const Common::DBConnectionInfo dbConnectionInfo, QObject* parent = nullptr);
    ~TanksConfig();

    bool loadFromDB();

    TankConfig* getTankConfig(const LevelGaugeService::TankID& id) const;
    TankIDList getTanksID() const;
    bool isExist(const LevelGaugeService::TankID& id) const;

signals:
    void errorOccurred(Common::EXIT_CODE errorCode, const QString& errorString);
    void sendLogMsg(Common::TDBLoger::MSG_CODE category, const QString &msg);

private slots:
    void lastMeasuments(const TankID& id, const QDateTime& lastTime);
    void lastSave(const TankID& id, const QDateTime& lastTime);
    void lastIntake(const TankID& id, const QDateTime& lastTime);

private:
    void lastUpdate(const QString& fieldName, const TankID &id, const QDateTime &lastTime);

private:
    const Common::DBConnectionInfo _dbConnectionInfo;
    QSqlDatabase _db;

    std::unordered_map<TankID, std::unique_ptr<TankConfig>> _tanksConfig;

};

} //namespace LevelGaugeService

#endif // TANKSCONFIG_H
