#ifndef CORE_H
#define CORE_H

//STL
#include <memory>
#include <vector>

//QT
#include <QObject>
#include <QList>
#include <QThread>

//My
#include "Common/tdbloger.h"
#include "tconfig.h"
#include "tankconfig.h"
#include "tanks.h"
#include "sync.h"

namespace LevelGaugeService
{

class Core
    : public QObject
{
    Q_OBJECT

public:
    Core(const Core&) = delete;
    Core& operator =(const Core&) = delete;
    Core(const Core&&) = delete;
    Core& operator =(const Core&&) = delete;

    explicit Core(QObject *parent = nullptr);
    ~Core();

    void start();
    void stop();

signals:
    void errorOccurred(Common::EXIT_CODE errorCode, const QString& errorString);
    void stopAll();

private slots:
    void errorOccurredTankConfig(Common::EXIT_CODE errorCode, const QString& errorString);
    void errorOccurredTanks(Common::EXIT_CODE errorCode, const QString& errorString);
    void errorOccurredSync(Common::EXIT_CODE errorCode, const QString& errorString);

private:
    bool startTankConfig();
    bool startTanks();
    bool startSync();

private:
    TConfig* _cnf = nullptr;
    Common::TDBLoger* _loger = nullptr;

    std::unique_ptr<TanksConfig> _tanksConfig;  //список конфигураций резервуаров

    struct TanksThread
    {
        std::unique_ptr<QThread> thread;
        std::unique_ptr<LevelGaugeService::Tanks> tanks;
    };

    std::unique_ptr<TanksThread> _tanks;

    struct SyncThread
    {
        std::unique_ptr<QThread> thread;
        std::unique_ptr<LevelGaugeService::Sync> sync;
    };

    std::unique_ptr<SyncThread> _sync; //поток синхронизации с БД АО НИТ

};

} // namespace LevelGaugeService

#endif // CORE_H
