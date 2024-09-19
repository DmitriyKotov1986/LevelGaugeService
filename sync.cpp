#include "syncdbstatus.h"
#include "syncdbintake.h"
#include "synchttpstatus.h"
#include "synchttpintake.h"

#include "sync.h"

using namespace LevelGaugeService;

SyncImpl::SyncImpl(QObject *parent)
    : QObject{parent}
{
}

SyncImpl::~SyncImpl()
{
}

void SyncImpl::calculateStatuses(const LevelGaugeService::TankID& id, const TankStatusesList &tankStatuses)
{
}

void SyncImpl::calculateIntakes(const LevelGaugeService::TankID& id, const IntakesList &intakes)
{
}

void SyncImpl::start()
{
}

void SyncImpl::stop()
{
}

Sync::Sync(const Common::DBConnectionInfo& dbConnectionInfo, TanksConfig* tanksConfig, QObject* parent /* = nullptr */)
    : QObject{parent}
{
    Q_CHECK_PTR(tanksConfig);

    //HTTP Status
    auto syncHTTPStatus = std::make_unique<SyncHTTPStatus>(dbConnectionInfo, tanksConfig);

    QObject::connect(syncHTTPStatus.get(), SIGNAL(errorOccurred(const QString&, Common::EXIT_CODE, const QString&)),
                     SLOT(errorOccurredSync(const QString&, Common::EXIT_CODE, const QString&)));
    QObject::connect(syncHTTPStatus.get(), SIGNAL(sendLogMsg(const QString&, Common::TDBLoger::MSG_CODE, const QString&)),
                     SLOT(sendLogMsgSync(const QString&, Common::TDBLoger::MSG_CODE, const QString&)));

    _syncList.emplace_back(std::move(syncHTTPStatus));

    //HTTP Intake
    auto syncHTTPIntake = std::make_unique<SyncHTTPIntake>(dbConnectionInfo, tanksConfig);

    QObject::connect(syncHTTPIntake.get(), SIGNAL(errorOccurred(const QString&, Common::EXIT_CODE, const QString&)),
                     SLOT(errorOccurredSync(const QString&, Common::EXIT_CODE, const QString&)));
    QObject::connect(syncHTTPIntake.get(), SIGNAL(sendLogMsg(const QString&, Common::TDBLoger::MSG_CODE, const QString&)),
                     SLOT(sendLogMsgSync(const QString&, Common::TDBLoger::MSG_CODE, const QString&)));

    _syncList.emplace_back(std::move(syncHTTPIntake));

    //DB Status
    auto syncDBStatus = std::make_unique<SyncDBStatus>(dbConnectionInfo, tanksConfig);

    QObject::connect(syncDBStatus.get(), SIGNAL(errorOccurred(const QString&, Common::EXIT_CODE, const QString&)),
                     SLOT(errorOccurredSync(const QString&, Common::EXIT_CODE, const QString&)));
    QObject::connect(syncDBStatus.get(), SIGNAL(sendLogMsg(const QString&, Common::TDBLoger::MSG_CODE, const QString&)),
                     SLOT(sendLogMsgSync(const QString&, Common::TDBLoger::MSG_CODE, const QString&)));

    _syncList.emplace_back(std::move(syncDBStatus));

    //DB Intake
    auto syncDBIntake = std::make_unique<SyncDBIntake>(dbConnectionInfo, tanksConfig);

    QObject::connect(syncDBIntake.get(), SIGNAL(errorOccurred(const QString&, Common::EXIT_CODE, const QString&)),
                     SLOT(errorOccurredSync(const QString&, Common::EXIT_CODE, const QString&)));
    QObject::connect(syncDBIntake.get(), SIGNAL(sendLogMsg(const QString&, Common::TDBLoger::MSG_CODE, const QString&)),
                     SLOT(sendLogMsgSync(const QString&, Common::TDBLoger::MSG_CODE, const QString&)));

    _syncList.emplace_back(std::move(syncDBIntake));
}

Sync::~Sync()
{
    stop();
}

void Sync::calculateStatuses(const LevelGaugeService::TankID& id, const TankStatusesList &tankStatuses)
{
     Q_ASSERT(_isStarted);

    for (auto& sync: _syncList)
    {
        sync->calculateStatuses(id, tankStatuses);
    }
}

void Sync::calculateIntakes(const LevelGaugeService::TankID& id, const IntakesList &intakes)
{
     Q_ASSERT(_isStarted);

    for (auto& sync: _syncList)
    {
        sync->calculateIntakes(id, intakes);
    }
}

void Sync::start()
{
    Q_ASSERT(!_isStarted);

    for (auto& sync: _syncList)
    {
        sync->start();
    }

    _isStarted = true;
}

void Sync::stop()
{
    if (!_isStarted)
    {
        return;
    }

    for (auto& sync: _syncList)
    {
        sync->stop();
    }

    _syncList.clear();

    _isStarted = false;
}

void Sync::errorOccurredSync(const QString &syncName, Common::EXIT_CODE errorCode, const QString &errorString)
{
    emit errorOccurred(errorCode, QString("Data synchronization error in module %1: %2").arg(syncName).arg(errorString));
}

void Sync::sendLogMsgSync(const QString &syncName, Common::TDBLoger::MSG_CODE category, const QString &msg)
{
    emit sendLogMsg(category, QString("Synchronization module %1: %2").arg(syncName).arg(msg));
}




