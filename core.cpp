//QT
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QSqlError>

//My
#include "Common/common.h"
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

bool Core::startTankConfig()
{
    Q_CHECK_PTR(_loger);
    Q_ASSERT(_tanksConfig == nullptr);

    _tanksConfig = std::make_unique<TanksConfig>(_cnf->dbConnectionInfo());

    QObject::connect(_tanksConfig.get(), SIGNAL(errorOccurred(Common::EXIT_CODE, const QString&)),
                     SLOT(errorOccurredTankConfig(Common::EXIT_CODE, const QString&)));
    QObject::connect(_tanksConfig.get(), SIGNAL(sendLogMsg(Common::TDBLoger::MSG_CODE, const QString&)),
                     _loger, SLOT(sendLogMsg(Common::TDBLoger::MSG_CODE, const QString&)));

    return _tanksConfig->loadFromDB();
}

bool Core::startSync()
{
    Q_ASSERT(_sync == nullptr);

    _sync = std::make_unique<Sync>(_cnf->dbConnectionInfo(), _tanksConfig.get());

    QObject::connect(_sync.get(), SIGNAL(errorOccurred(Common::EXIT_CODE, const QString&)),
                     SLOT(errorOccurredSync(Common::EXIT_CODE, const QString&)));
    QObject::connect(_sync.get(), SIGNAL(sendLogMsg(Common::TDBLoger::MSG_CODE, const QString&)),
                     _loger, SLOT(sendLogMsg(Common::TDBLoger::MSG_CODE, const QString&)));

    _sync->start();

    return true;
}

bool Core::startTanks()
{
    Q_CHECK_PTR(_tanksConfig);
    Q_CHECK_PTR(_sync);

    _tanks = std::make_unique<Tanks>(_cnf->dbConnectionInfo(), _tanksConfig.get());

    QObject::connect(_tanks.get(), SIGNAL(errorOccurred(Common::EXIT_CODE, const QString&)),
                     SLOT(errorOccurredTanks(Common::EXIT_CODE, const QString&)), Qt::QueuedConnection);
    QObject::connect(_tanks.get(), SIGNAL(sendLogMsg(Common::TDBLoger::MSG_CODE, const QString&)),
                     _loger, SLOT(sendLogMsg(Common::TDBLoger::MSG_CODE, const QString&)), Qt::QueuedConnection);

    QObject::connect(_tanks.get(), SIGNAL(calculateStatuses(const LevelGaugeService::TankID&, const TankStatusesList&)),
                     _sync.get(), SLOT(calculateStatuses(const LevelGaugeService::TankID&, const TankStatusesList&)), Qt::QueuedConnection);
    QObject::connect(_tanks.get(), SIGNAL(calculateIntakes(const LevelGaugeService::TankID&, const IntakesList&)),
                     _sync.get(), SLOT(calculateIntakes(const LevelGaugeService::TankID&, const IntakesList&)), Qt::QueuedConnection);

    _tanks->start();

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
    //Tanks
    if (_tanks)
    {
        _tanks->stop();
    }
    _tanks.reset(nullptr);

    if (_sync)
    {
        _sync->stop();
    }
    _sync.reset(nullptr);

    //TanksConfig
    _tanksConfig.reset(nullptr);
}

void Core::errorOccurredTankConfig(Common::EXIT_CODE errorCode, const QString &errorString)
{
    stop();

    emit errorOccurred(errorCode, QString("Error tanks config: %1").arg(errorString));
}

void Core::errorOccurredTanks(Common::EXIT_CODE errorCode, const QString &errorString)
{
    stop();

    emit errorOccurred(errorCode, QString("Error tanks: %1").arg(errorString));
}

void Core::errorOccurredSync(Common::EXIT_CODE errorCode, const QString &errorString)
{
    stop();

    emit errorOccurred(errorCode, QString("Error sync: %1").arg(errorString));
}
