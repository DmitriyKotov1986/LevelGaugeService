//Qt
#include <QMutex>
#include <QMutexLocker>
#include <QSqlError>
#include <QSqlQuery>

#ifdef QT_DEBUG
#include <QFile>
#include <QTextStream>
#endif

#include "tank.h"

using namespace LevelGaugeService;

static const int MIN_STEP_COUNT_START_INTAKE = 5;
static const int MIN_STEP_COUNT_FINISH_INTAKE = 10;

static const int AZS_CONNECTION_TIMEOUT = 660;



int connectionNumber()
{
    static QMutex mutex;
    static int connectionNumberValue = 0;

    QMutexLocker locker(&mutex);

    return ++connectionNumberValue;
}

Tank::Tank(const TankID& id, const Common::DBConnectionInfo& dbConnectionInfo,  QObject *parent /* = nullptr) */)
    : QObject{parent}
    , _dbConnectionInfo(dbConnectionInfo)
    , _dbConnectionName(QString("DB_%1").arg(connectionNumber()))
    , rg(QRandomGenerator::global())
{
    Q_ASSERT(!id.levelGaugeCode.isEmpty());
    Q_ASSERT(id.tankNumber != 0);
    Q_ASSERT(!_dbConnectionInfo.db_DBName.isEmpty());
    Q_ASSERT(!_dbConnectionInfo.db_Driver.isEmpty());

    _tankConfig.id = id;
}

Tank::~Tank()
{
    delete _timer;

    if (_db.isOpen())
    {
        _db.close();
    }

    QSqlDatabase::removeDatabase(_dbConnectionName);
}

void Tank::start()
{
    if (!connectToDB())
    {
        emit errorOccurred(QString("Cannot connect to database. Error: %1").arg(_db.lastError().text()));

        return;
    };

    loadTankConfig();
    makeLimits();
    initFromSave();

    //создаем основной рабочий таймер
    _timer = new QTimer();
    _timer->setSingleShot(false);

    QObject::connect(_timer, SIGNAL(timeout()), SLOT(calculate()));

    _timer->start(60000 + (rg->bounded(200) - 100));

    calculate();
}

void Tank::stop()
{

    emit finished();
}

bool Tank::connectToDB()
{
    Q_ASSERT(!_db.isOpen());

    //настраиваем подключение БД
    _db = QSqlDatabase::addDatabase(_dbConnectionInfo.db_Driver, _dbConnectionName);
    _db.setDatabaseName(_dbConnectionInfo.db_DBName);
    _db.setUserName(_dbConnectionInfo.db_UserName);
    _db.setPassword(_dbConnectionInfo.db_Password);
    _db.setConnectOptions(_dbConnectionInfo.db_ConnectOptions);
    _db.setPort(_dbConnectionInfo.db_Port);
    _db.setHostName(_dbConnectionInfo.db_Host);

    //подключаемся к БД
    return _db.open();
}

void Tank::dbQueryExecute(QSqlQuery &query, const QString &queryText)
{
    Q_ASSERT(_db.isOpen());

    if (!query.exec(queryText))
    {
        errorDBQuery(query);
    }
}

void Tank::errorDBQuery(const QSqlQuery &query)
{
    Q_ASSERT(_db.isOpen());

    emit errorOccurred(QString("Cannot execute query. Error: %1. Query: %2").arg(query.lastError().text()).arg(query.lastQuery()));

    _db.rollback();
}

void Tank::dbQueryExecute(const QString &queryText)
{
    Q_ASSERT(_db.isOpen());

    _db.transaction();
    QSqlQuery query(_db);

    dbQueryExecute(query, queryText);

    dbCommit();
}

void Tank::dbCommit()
{
    Q_ASSERT(_db.isOpen());

    if (!_db.commit())
    {
        emit errorOccurred(QString("Cannot commit trancsation. Error: %1").arg(_db.lastError().text()));

        _db.rollback();
    }
}

void Tank::loadTankConfig()
{
    Q_ASSERT(_db.isOpen());

    _db.transaction();
    QSqlQuery query(_db);
    query.setForwardOnly(true);

    //загружаем данные об АЗС
    const QString queryText =
            QString("SELECT [AZSCode], [TankNumber], [ServiceMode], [Volume], [Diametr], [Product], [LastSaveDateTime], [LastIntakeDateTime], [TimeShift], [LevelGaugeServiceDB] "
                    "FROM [TanksInfo] "
                    "WHERE [ID] = %1").arg(_tankConfig.id);

    dbQueryExecute(query, queryText);

    if (query.next())
    {
        _tankConfig.AZSCode = query.value("AZSCode").toString();
        _tankConfig.tankNumber = query.value("TankNumber").toUInt();
        _tankConfig.serviceMode = query.value("ServiceMode").toBool();
        _tankConfig.dbNitName = query.value("LevelGaugeServiceDB").toString();//имя БД для сохранения результатов
        _tankConfig.lastIntake = query.value("LastIntakeDateTime").toDateTime();
        _tankConfig.timeShift = query.value("TimeShift").toInt(); //cмещение времени относительно сервера
        _tankConfig.product =  query.value("Productr").toString();
        _tankConfig.diametr = query.value("Diametr").toFloat();
        _tankConfig.volume = query.value("Volume").toFloat();
    }
    else
    {
        emit errorOccurred(QString("Cannot load tank configuration with ID = %1").arg(_tankConfig.id));
    }

    dbCommit();
}

void Tank::makeLimits()
{
    _limits.density = std::make_pair<float>(350.0, 1200.0);
    _limits.height  = std::make_pair<float>(0.0, _tankConfig.diametr);
    _limits.mass    = std::make_pair<float>(0.0, _tankConfig.volume * _limits.density.second);
    _limits.volume  = std::make_pair<float>(0.0, _tankConfig.volume);
    _limits.temp    = std::make_pair<float>(-50.0, 100.0);
}

void Tank::initFromSave()
{
    //т.к. приоритетное значение имеет сохранненные измерения - то сначала загружаем их
    _db.transaction();
    QSqlQuery query(_db);
    query.setForwardOnly(true);

    const auto queryText =
        QString("SELECT [TankNumber], [DateTime], [Volume], [Mass], [Density], [Height], [Temp], [Flag] "
                "FROM [TanksStatus] "
                "WHERE [AZSCode] = '%1' AND [TankNumber] = %2 AND [DateTime] >= CAST('%3' AS DATETIME2)")
            .arg(_tankConfig.AZSCode)
            .arg(_tankConfig.tankNumber)
            .arg(_tankConfig.lastIntake.addSecs(-600).toString("yyyy-MM-dd hh:mm:ss.zzz"));

    dbQueryExecute(query, queryText);

    //сохраняем
    bool added = false;
    while (query.next())
    {
        auto tmp = std::make_unique<Status>();
        tmp->density = query.value("Density").toFloat();
        tmp->height = query.value("Height").toFloat() * 10; //переводим высоту обратно в мм
        tmp->mass = query.value("Mass").toFloat();
        tmp->temp = query.value("Temp").toFloat();
        tmp->volume = query.value("Volume").toFloat();
        tmp->flag = query.value("Flag").toUInt();

        _tankResultStatus.insert({query.value("DateTime").toDateTime().addSecs(-_tankConfig.timeShift), std::move(tmp)}); //время переводим во время сервера

        added = true;
    }

    if (added)
    {
        const auto lastDateTime = _tankResultStatus.rbegin()->first;
        _tankConfig.lastSave = lastDateTime;
        _tankConfig.lastMeasuments = lastDateTime;
    }
    else
    {
        _tankConfig.lastSave = _tankConfig.lastIntake;
        _tankConfig.lastMeasuments = _tankConfig.lastIntake;
    }

    dbCommit();
}

void Tank::loadFromMeasument()
{
    _db.transaction();
    QSqlQuery query(_db);
    query.setForwardOnly(true);

    const auto queryText =
        QString("SELECT [ID], [DateTime], [Density], [Height], [Volume], [Mass], [Temp] "
                "FROM [TanksMeasument] "
                "WHERE [AZSCode] = '%1' AND [TankNumber] = %2 AND [DateTime] > CAST('%3' AS DATETIME2) ")
            .arg(_tankConfig.AZSCode)
            .arg(_tankConfig.tankNumber)
            .arg(_tankConfig.lastMeasuments.toString("yyyy-MM-dd hh:mm:ss.zzz"));

    dbQueryExecute(query, queryText);

    while (query.next())
    {
        auto tmp = std::make_unique<Status>();
        tmp->density = query.value("Density").toFloat();
        tmp->height = query.value("Height").toFloat();
        tmp->mass = query.value("Mass").toFloat();
        tmp->temp = query.value("Temp").toFloat();
        tmp->volume = query.value("Volume").toFloat();
        tmp->flag = static_cast<quint8>(AdditionFlag::MEASUMENTS);

        _tankTargetStatus.insert({query.value("DateTime").toDateTime().addSecs(-_tankConfig.timeShift), std::move(tmp)});//время переводим во время сервера

        _tankConfig.lastMeasuments = std::max(_tankConfig.lastMeasuments, query.value("DateTime").toDateTime());
    }

    dbCommit();
}

void Tank::calculate()
{
    if (!_db.isOpen())
    {
        emit errorOccurred(QString("Connetion to DB %1 not open. Skip.").arg(_db.connectionName()));

        return;
    }

    //загружаем данные из БД с измерениями
    loadFromMeasument();

    //если никаких данных нет - выходим
    if (_tankTargetStatus.empty() && _tankResultStatus.empty())
    {
        emit errorOccurred("No data for work");

        return;
    }

    //проверяем и добавляем недостающие записи статусов
    makeResultStatus();

    saveToNitDB();

    saveIntake();

  //  _tankConfig.lastCheck = _tankStatus.rbegin()->first;
}

void Tank::makeResultStatus()
{
    //если никаких данных нет - то выходим
    if (_tankTargetStatus.empty() && _tankResultStatus.empty())
    {
        return;
    }

    //если результирующая карта пустая - то записываем в нее первое целевое значение
    if (_tankResultStatus.empty())
    {
        _tankResultStatus.insert({_tankTargetStatus.begin()->first,
                                  std::make_unique<Status>(*(_tankTargetStatus.begin()->second))});
    }

    TankStatus::const_iterator tankTargetStatus_it;
    do
    {
        //находим ближайшую цель
        const auto lastResultDateTime = _tankResultStatus.rbegin()->first;
        tankTargetStatus_it = _tankTargetStatus.upper_bound(lastResultDateTime);

        if (tankTargetStatus_it != _tankTargetStatus.end())
        {
            const auto& lastResultHeight = _tankResultStatus.rbegin()->second->height;
            const auto& targetHeight = tankTargetStatus_it->second->height;

            if ((targetHeight - lastResultHeight) > DELTA_INTAKE_VOLUME)
            {
                addStatusIntake(*(tankTargetStatus_it->second.get()));
            }
            else
            {
                addStatusRange(tankTargetStatus_it->first, *(tankTargetStatus_it->second.get()));
            }

            checkLimits(_tankResultStatus.upper_bound(lastResultDateTime));
        }
    }
    while (tankTargetStatus_it != _tankTargetStatus.end());

    //если с конца осутствует более AZS_CONNECTION_TIMEOUT секунд - то скорее всего обрыв связи с АЗС, поэтому добавляем
    //данные вручную
    if (_tankResultStatus.rbegin()->first.secsTo(QDateTime::currentDateTime()) > AZS_CONNECTION_TIMEOUT)
    {
        addStatusEnd();
    }
}

void Tank::checkLimits(TankStatusIterator start_it)
{
    //Проверяем лимитные ограничения
    for (auto tankStatus_it = start_it; tankStatus_it != _tankResultStatus.end(); ++tankStatus_it)
    {
        auto status = tankStatus_it->second.get();

        //density
        if (status->density < _limits.density.first)
        {
            status->density = _limits.density.first;
            status->flag = status->flag | static_cast<quint8>(AdditionFlag::CORRECTED);
        }
        else if (status->density > _limits.density.second)
        {
            status->density = _limits.density.second;
            status->flag = status->flag | static_cast<quint8>(AdditionFlag::CORRECTED);
        }

        //height
        if (status->height < _limits.height.first)
        {
            status->height = _limits.height.first;
            status->flag = status->flag | static_cast<quint8>(AdditionFlag::CORRECTED);
        }
        else if (status->height > _limits.height.second)
        {
            status->height = _limits.height.second;
            status->flag = status->flag | static_cast<quint8>(AdditionFlag::CORRECTED);
        }

        //mass
        if (status->mass < _limits.mass.first)
        {
            status->mass = _limits.mass.first;
            status->flag = status->flag | static_cast<quint8>(AdditionFlag::CORRECTED);
        }
        else if (status->mass > _limits.mass.second)
        {
            status->mass = _limits.mass.second;
            status->flag = status->flag | static_cast<quint8>(AdditionFlag::CORRECTED);
        }

        //volume
        if (status->volume < _limits.volume.first)
        {
            status->volume = _limits.volume.first;
            status->flag = status->flag | static_cast<quint8>(AdditionFlag::CORRECTED);
        }
        else if (status->volume > _limits.volume.second)
        {
            qDebug() << status->volume << _limits.volume.second;
            status->volume = _limits.volume.second;
            status->flag = status->flag | static_cast<quint8>(AdditionFlag::CORRECTED);
        }

        //temp
        if (status->temp < _limits.temp.first)
        {
            status->temp = _limits.temp.first;
            status->flag = status->flag | static_cast<quint8>(AdditionFlag::CORRECTED);
        }
        else if (status->temp > _limits.temp.second)
        {
            status->temp = _limits.temp.second;
            status->flag = status->flag | static_cast<quint8>(AdditionFlag::CORRECTED);
        }
    }
}

void Tank::addStatusRange(const QDateTime& targetDateTime, const Status& targetStatus)
{
    Q_ASSERT(!_tankResultStatus.empty());

    const Status startStatus(*(_tankResultStatus.rbegin()->second.get()));

    //время на текущем шаге
    auto time = _tankResultStatus.rbegin()->first;

    //количество шагов которое у нас есть для вставки
    int stepCount = static_cast<int>(static_cast<double>(time.secsTo(targetDateTime)) / 60.0) - 1;

    //вычисляем дельту
    Delta delta;

    do
    {
        delta.height = (targetStatus.height - startStatus.height) / static_cast<float>(stepCount + 1);
        delta.density = (targetStatus.density - startStatus.density) / static_cast<float>(stepCount + 1);
        delta.temp = (targetStatus.temp - startStatus.temp) / static_cast<float>(stepCount + 1);
        delta.volume = (targetStatus.volume - startStatus.volume) / static_cast<float>(stepCount + 1);
        delta.mass = (targetStatus.mass - startStatus.mass) / static_cast<float>(stepCount + 1);

        //Если дельты слишком большие - то увеличиваем количество необходимых шагов
        if ((std::abs(delta.height)  >= DELTA_MAX.height)  ||
            (std::abs(delta.density) >= DELTA_MAX.density) ||
            (std::abs(delta.temp)    >= DELTA_MAX.temp)    ||
            (std::abs(delta.volume)  >= DELTA_MAX.volume)  ||
            (std::abs(delta.mass)    >= DELTA_MAX.mass ))

        {
            ++stepCount;
        }
        else
        {
            break;
        }
    }
    while (true);

    auto tmp = Status(startStatus); //статус на текущем шаге
    tmp.flag = static_cast<quint8>(AdditionFlag::CALCULATE);

    for (auto i = 0; i < stepCount; ++i)
    {
        tmp.density += delta.density;
        tmp.height += delta.height;
        tmp.temp += delta.temp;
        tmp.volume += delta.volume;
        tmp.mass += delta.mass;

        time = time.addSecs(60);
        
        auto tankStatus_it = _tankResultStatus.insert({time.addMSecs(rg->bounded(-1000, +1000)), std::make_unique<Status>(tmp)}).first;

        //добавить рандомный +/-
        addRandom(tankStatus_it);
    }

    _tankResultStatus.insert({time.addSecs(60), std::make_unique<Status>(targetStatus)});
}

void Tank::addStatusIntake(const Status& targetStatus)
{
    Q_ASSERT(!_tankResultStatus.empty());

    //время на текущем шаге
    auto time = _tankResultStatus.rbegin()->first;

    //расчитываем сколько шагов нужно для подъема уровня
    const auto startStatus = *(_tankResultStatus.rbegin()->second.get());

    const int stepCount = static_cast<int>((targetStatus.height - startStatus.height) / (DELTA_MAX_INTAKE.height * 0.95));
    
    auto tmp = startStatus;
    tmp.flag = static_cast<quint8>(AdditionFlag::CALCULATE);

    //вставляем полочку в начале
    for (auto i = 0; i < MIN_STEP_COUNT_START_INTAKE; ++i)
    {
        time = time.addSecs(60);

        auto tankStatus_it = _tankResultStatus.insert({time.addMSecs(rg->bounded(-1000, +1000)), std::make_unique<Status>(tmp)}).first;

        //добавить рандомный +/-
        addRandom(tankStatus_it);
    }

    //вставляем подъем
    Delta delta;
    delta.density = (targetStatus.density - startStatus.density) / static_cast<float>(stepCount);
    delta.temp = (targetStatus.temp - startStatus.temp) / static_cast<float>(stepCount);
    delta.volume = (targetStatus.volume - startStatus.volume) / static_cast<float>(stepCount);
    delta.mass = (targetStatus.mass - startStatus.mass) / static_cast<float>(stepCount);

    for (auto i = 0; i < stepCount; ++i)
    {
        tmp.density += delta.density;
        tmp.height += delta.height;
        tmp.temp += delta.temp;
        tmp.volume += delta.volume;
        tmp.mass += delta.mass;

        time = time.addSecs(60);

        auto tankStatus_it = _tankResultStatus.insert({time.addMSecs(rg->bounded(-1000, +1000)), std::make_unique<Status>(tmp)}).first;

        //добавить рандомный +/-
        addRandom(tankStatus_it);
    }

    //вставляем полочку в конце
    for (auto i = 0; i < MIN_STEP_COUNT_FINISH_INTAKE; ++i)
    {
        time = time.addSecs(60);

        auto tankStatus_it = _tankResultStatus.insert({time.addMSecs(rg->bounded(-1000, +1000)), std::make_unique<Status>(tmp)}).first;

        //добавить рандомный +/-
        addRandom(tankStatus_it);
    }

    _tankResultStatus.insert({time.addMSecs(rg->bounded(-1000, +1000)), std::make_unique<Status>(targetStatus)});
}

void Tank::addStatusEnd()
{
    Q_ASSERT(!_tankResultStatus.empty());

    auto time = _tankResultStatus.rbegin()->first;  //время на текущем шаге
    auto tmp = *(_tankResultStatus.rbegin()->second.get());
    tmp.flag = static_cast<quint8>(AdditionFlag::CALCULATE);

    while (time.secsTo(QDateTime::currentDateTime()) >= AZS_CONNECTION_TIMEOUT)
    {
        time = time.addSecs(60);

        auto tankResultStatus_it = _tankResultStatus.insert({time.addMSecs(rg->bounded(-1000, +1000)) , std::make_unique<Status>(tmp)}).first;

        //добавить рандомный +/-
        addRandom(tankResultStatus_it);
    }
}

void Tank::addRandom(Tank::TankStatusIterator it)
{
    it->second->density += round(static_cast<float>(rg->bounded(-100, +100)) / 200.0f) * 0.1f;
    it->second->height  += round(static_cast<float>(rg->bounded(-100, +100)) / 200.0f) * 1.0f;
    it->second->temp    += round(static_cast<float>(rg->bounded(-100, +100)) / 200.0f) * 0.1f;
    it->second->volume  += round(static_cast<float>(rg->bounded(-100, +100)) / 200.0f) * 10.0f;
    it->second->mass    += round(static_cast<float>(rg->bounded(-100, +100)) / 200.0f) * 10.0f;
}

