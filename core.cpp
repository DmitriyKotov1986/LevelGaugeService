//QT
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QSqlError>

//My
#include "common/common.h"
#include "core.h"

using namespace LevelGaugeService;
using namespace Common;

static const QString CORE_CONNECTION_TO_DB_NAME = "CoreDB";

Core::Core(QObject *parent)
    : QObject{parent}
    , _cnf(TConfig::config())
    , _loger(Common::TDBLoger::DBLoger())
{
    Q_CHECK_PTR(_cnf);
    Q_CHECK_PTR(_loger);
}

Core::~Core()
{
}

bool Core::startSync()
{
    Q_CHECK_PTR(_tanksConfig);

    _sync = std::make_unique<SyncThread>();

    _sync->sync = std::make_unique<Sync>(_cnf->dbConnectionInfo(), _cnf->dbNitConnectionInfo(), _tanksConfig.get());
    _sync->thread = std::make_unique<QThread>();

    _sync->sync->moveToThread(_sync->thread.get());

    QObject::connect(_sync->thread.get(), SIGNAL(started()), _sync->sync.get(), SLOT(start()), Qt::DirectConnection);
    QObject::connect(_sync->sync.get(), SIGNAL(finished()), _sync->thread.get(), SLOT(quit()), Qt::DirectConnection);

    QObject::connect(this, SIGNAL(stopAll), _sync->sync.get(), SLOT(stop()), Qt::QueuedConnection);

    QObject::connect(_sync->sync.get(), SIGNAL(errorOccurred(Common::EXIT_CODE errorCode, const QString& errorString)),
                     SLOT(errorOccurredSync(Common::EXIT_CODE errorCode, const QString& errorString)), Qt::QueuedConnection);
    QObject::connect(_sync->sync.get(), SIGNAL(sendLogMsg(Common::TDBLoger::MSG_CODE category, const QString &msg)),
                     _loger, SLOT(sendLogMsg(Common::TDBLoger::MSG_CODE category, const QString &msg)), Qt::QueuedConnection);

    _sync->thread->start();

    return true;
}

bool Core::startTankConfig()
{
    Q_CHECK_PTR(_loger);
    Q_ASSERT(_tanksConfig == nullptr);

    _tanksConfig = std::make_unique<TanksConfig>(_cnf->dbConnectionInfo());

    QObject::connect(_tanksConfig.get(), SIGNAL(errorOccurred(Common::EXIT_CODE errorCode, const QString& errorString)),
                     SLOT(errorOccurredTankConfig(Common::EXIT_CODE errorCode, const QString& errorString)));
    QObject::connect(_tanksConfig.get(), SIGNAL(sendLogMsg(Common::TDBLoger::MSG_CODE category, const QString &msg)),
                     _loger, SLOT(sendLogMsg(Common::TDBLoger::MSG_CODE category, const QString &msg)));

    return _tanksConfig->loadFromDB();
}

bool Core::startTanks()
{
    Q_CHECK_PTR(_tanksConfig);
    Q_CHECK_PTR(_sync);

    _tanks = std::make_unique<TanksThread>();

    _tanks->tanks = std::make_unique<Tanks>(_cnf->dbConnectionInfo(), _tanksConfig.get());
    _tanks->thread = std::make_unique<QThread>();

    _tanks->tanks->moveToThread(_tanks->thread.get());

    QObject::connect(_tanks->thread.get(), SIGNAL(started()), _tanks->tanks.get(), SLOT(start()), Qt::DirectConnection);
    QObject::connect(_tanks->tanks.get(), SIGNAL(finished()), _tanks->thread.get(), SLOT(quit()), Qt::DirectConnection);

    QObject::connect(this, SIGNAL(stopAll), _tanks->tanks.get(), SLOT(stop()), Qt::QueuedConnection);

    QObject::connect(_tanks->tanks.get(), SIGNAL(errorOccurred(Common::EXIT_CODE errorCode, const QString& errorString)),
                     SLOT(errorOccurredTanks(Common::EXIT_CODE errorCode, const QString& errorString)), Qt::QueuedConnection);
    QObject::connect(_tanks->tanks.get(), SIGNAL(sendLogMsg(Common::TDBLoger::MSG_CODE category, const QString &msg)),
                     _loger, SLOT(sendLogMsg(Common::TDBLoger::MSG_CODE category, const QString &msg)), Qt::QueuedConnection);

    QObject::connect(_tanks->tanks.get(), SIGNAL(newStatuses(const LevelGaugeService::TankID&, const LevelGaugeService::TankStatusesList&)),
                     _sync->sync.get(), SLOT(newStatuses(const LevelGaugeService::TankID&, const LevelGaugeService::TankStatusesList&)), Qt::QueuedConnection);
    QObject::connect(_tanks->tanks.get(), SIGNAL(newIntakes(const LevelGaugeService::TankID&, const LevelGaugeService::IntakesList&)),
                     _sync->sync.get(), SLOT(newIntakes(const LevelGaugeService::TankID&, const LevelGaugeService::IntakesList&)), Qt::QueuedConnection);

    _tanks->thread->start();

    return true;
}

void Core::start()
{
    //загружаем конфигурацию
    if (!startTankConfig())
    {
        // в случае ошибки нам прилетит errorOccurredTankConfig(...)
        return;
    }

    if (!startSync())
    {
        // в случае ошибки нам прилетит errorOccurredSync(...)
        return;
    }

    if (!startTanks())
    {
        // в случае ошибки нам прилетит errorOccurredTanks(...)
        return;
    }
}

void Core::stop()
{
    emit stopAll();

    //Sync
    if (_sync)
    {
        _sync->thread->wait();
    }

    _sync.reset(nullptr);

    //Tanks
    if (_tanks)
    {
        _tanks->thread->wait();
    }
    _tanks.reset(nullptr);

    //TanksConfig
    _tanksConfig.reset(nullptr);
}

void Core::errorOccurredTankConfig(Common::EXIT_CODE errorCode, const QString &errorString)
{
    emit errorOccurred(errorCode, QString("Error Tank config: %1").arg(errorString));
}

void Core::errorOccurredTanks(Common::EXIT_CODE errorCode, const QString &errorString)
{
    emit errorOccurred(errorCode, QString("Error Tanks: %1").arg(errorString));
}

void Core::errorOccurredSync(Common::EXIT_CODE errorCode, const QString &errorString)
{
    emit errorOccurred(errorCode, QString("Error Sync: %1").arg(errorString));
}
