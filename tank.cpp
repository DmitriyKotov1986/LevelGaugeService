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

static const float DELTA_INTAKE_VOLUME = 400.0; //пороговое значение изменения уровня топлива за 10 минут с котого считаем что произошел прием

static const Tank::Delta DELTA_MAX
{
    /*Volume*/40.0,
    /*Mass*/40.0,
    /*Density*/0.2,
    /*Height*/10.0,
    /*Temp*/0.1
}; //максимально допустимое изменение параметров резервуара за 1 минуту при нормальной работе

static const Tank::Delta DELTA_MAX_INTAKE
{
    /*Volume*/700.0,
    /*Mass*/600.0,
    /*Density*/0.5,
    /*Height*/70.0,
    /*Temp*/1.0
}; //максимально допустимое изменение параметров резервуара за 1 минуту при приеме топлива

static const int MIN_STEP_COUNT_START_INTAKE = 5;
static const int MIN_STEP_COUNT_FINISH_INTAKE = 10;

Tank::AdditionFlag IntToAdditionFlag(quint8 status)
{
    switch (status)
    {
    case static_cast<quint8>(Tank::AdditionFlag::MEASUMENTS): return Tank::AdditionFlag::MEASUMENTS;
    case static_cast<quint8>(Tank::AdditionFlag::CALCULATE): return Tank::AdditionFlag::CALCULATE;
    case static_cast<quint8>(Tank::AdditionFlag::MANUAL): return Tank::AdditionFlag::MANUAL;
    }
    return Tank::AdditionFlag::UNKNOWN;
}


Tank::AdditionFlag AdditionFlagIntToAdditionFlag(quint8 status)
{
    switch (status)
    {
    case static_cast<quint8>(Tank::AdditionFlag::MEASUMENTS): return Tank::AdditionFlag::MEASUMENTS;
    case static_cast<quint8>(Tank::AdditionFlag::CALCULATE): return Tank::AdditionFlag::CALCULATE;
    case static_cast<quint8>(Tank::AdditionFlag::MANUAL): return Tank::AdditionFlag::MANUAL;
    }
    return Tank::AdditionFlag::UNKNOWN;
}

int connectionNumber()
{
    static QMutex mutex;
    static int connectionNumberValue = 0;

    QMutexLocker locker(&mutex);

    return ++connectionNumberValue;
}

Tank::Tank(qint64 tankID, const Common::DBConnectionInfo& dbConnectionInfo,  QObject *parent /* = nullptr) */)
    : QObject{parent}
    , _dbConnectionInfo(dbConnectionInfo)
    , _dbConnectionName(QString("DB_%1").arg(connectionNumber()))
    , rg(QRandomGenerator::global())
{
    Q_ASSERT(tankID >= 0);
    Q_ASSERT(!_dbConnectionInfo.db_DBName.isEmpty());
    Q_ASSERT(!_dbConnectionInfo.db_Driver.isEmpty());

    _tankConfig.id = tankID;
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
            .arg(_tankConfig.lastIntake.toString("yyyy-MM-dd hh:mm:ss.zzz"));

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

        _tankStatus.insert({query.value("DateTime").toDateTime(), std::move(tmp)});

        added = true;
    }

    if (added)
    {
        const auto lastDateTime = _tankStatus.rbegin()->first;
        _tankConfig.lastSave = lastDateTime;
        _tankConfig.lastMeasuments = lastDateTime;
        _tankConfig.lastCheck = lastDateTime;
    }
    else
    {
        _tankConfig.lastSave = _tankConfig.lastIntake;
        _tankConfig.lastMeasuments = _tankConfig.lastIntake;
        _tankConfig.lastCheck = _tankConfig.lastIntake;
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

        _tankStatus.insert({query.value("DateTime").toDateTime(), std::move(tmp)});

        _tankConfig.lastMeasuments = query.value("DateTime").toDateTime();
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

    if (_tankStatus.empty())
    {
        return;
    }

    //проверяем и добавляем недостающие записи статусов
    checkStatus();
    checkLimits();

    saveToNitDB();

    saveIntake();
}

void Tank::checkStatus()
{
    if (_tankStatus.empty())
    {
        return;
    }

    //находим последний проверенный статус
    auto tankStatusStartCheck_it = _tankStatus.lower_bound(_tankConfig.lastCheck);
    if (tankStatusStartCheck_it == _tankStatus.end())
    {
        tankStatusStartCheck_it = addStatusStart(_tankConfig.lastCheck);
    }

    //если это не самый конец вектора - то проверяем все имеющися
    if (tankStatusStartCheck_it != _tankStatus.end())
    {
        //проверяем что доступны все необходимые записи в середине имяющихся записей
        for (auto tankStatus_it = std::next(tankStatusStartCheck_it); tankStatus_it != _tankStatus.end(); ++tankStatus_it)
        {
            const auto tankStatus_prev_it = std::prev(tankStatus_it);

            const auto tankStatusPrevDateTime = tankStatus_prev_it->first;
            const auto tankStatusDateTime = tankStatus_it->first;

            const quint64 deltaTime = tankStatusPrevDateTime.secsTo(tankStatusDateTime); //в сек
            //сравниваем время между соседнями записами. Если разрыв более 120 секунд - необходимо добавить недостающие записи
            if (deltaTime >= 118)
            {
                //Возможные варианты:
                //1. уровень уменьшился или остался прежний или увеличился менее допустимого(температурное расширение) - обычный режим работы АЗС
                //2. уровень вырос больше допустимого - был приход

                const auto deltaHeight = tankStatus_it->second->height - tankStatus_prev_it->second->height;
                if (deltaHeight > DELTA_INTAKE_VOLUME)
                {
                    tankStatus_it = addStatusIntake(tankStatus_prev_it, tankStatus_it);
                }
                else
                {
                    tankStatus_it = addStatusRange(tankStatus_prev_it, tankStatus_it);
                }
            }
        }
    }
    else
    {

    }

    //если с конца осутствует более 11 минут - то скорее всего обрыв связи с АЗС, поэтому добавляем
    //данные вручную

    const QDateTime currentDateTime = QDateTime::currentDateTime().addSecs(_tankConfig.timeShift); //Текущее время на АЗС
    if (_tankStatus.rbegin()->first.secsTo(currentDateTime) > 660)
    {
        addStatusEnd(currentDateTime);
    }
}

void Tank::checkLimits()
{
    auto tankStatusStartCheck_it = _tankStatus.lower_bound(_tankConfig.lastCheck);
    if (tankStatusStartCheck_it == _tankStatus.end())
    {
        return;
    }

    //проверяем что доступны все необходимые записи в середине имяющихся записей
    for (auto tankStatus_it = tankStatusStartCheck_it; tankStatus_it != _tankStatus.end(); ++tankStatus_it)
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
        else if (status->height> _limits.height.second)
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
        else if (status->mass> _limits.mass.second)
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
        else if (status->volume> _limits.volume.second)
        {
            status->volume = _limits.volume.second;
            status->flag = status->flag | static_cast<quint8>(AdditionFlag::CORRECTED);
        }

        //temp
        if (status->temp < _limits.temp.first)
        {
            status->temp = _limits.temp.first;
            status->flag = status->flag | static_cast<quint8>(AdditionFlag::CORRECTED);
        }
        else if (status->temp> _limits.temp.second)
        {
            status->temp = _limits.temp.second;
            status->flag = status->flag | static_cast<quint8>(AdditionFlag::CORRECTED);
        }
    }
}

Tank::TankStatusIterator Tank::addStatusRange(Tank::TankStatusIterator start, Tank::TankStatusIterator finish)
{
    Q_ASSERT(start != _tankStatus.end());
    Q_ASSERT(finish != _tankStatus.end());

    auto time = start->first;  //время на текущем шаге
    const auto timeFinish = finish->first;  //время на текущем шаге

    //вычисляем дельту (значения нужно именно скопироватть, т.к. итераторы могут стать недействительными)
    const auto startStatus(*start->second.get());
    auto finishStatus(*finish->second.get());

    //количество шагов которое у нас есть для вставки
    int startStepCount = static_cast<int>(static_cast<double>(time.secsTo(timeFinish)) / 60.0) - 1 ;
    startStepCount = startStepCount < 1 ? 1 : startStepCount;
    int stepCount= startStepCount;
    
    //Вычисляем необходимые дельты. Дельта слишком большая - придется перезаписать часть уже имеющихся данных
    Delta delta;

    do
    {
        delta.density = (finishStatus.density - startStatus.density) / static_cast<float>(stepCount);
        if (std::abs(delta.density) > DELTA_MAX.density)
        {
            stepCount = (finishStatus.density - startStatus.density) / DELTA_MAX.density;
            const int stepAddiotion = stepCount - startStepCount; //количество добавляемыш шагов
            if (std::distance(finish, _tankStatus.end()) < stepAddiotion)
            {
                finishStatus = *(std::next(finish, stepAddiotion)->second.get());
            }
            else
            {
                finishStatus = *(_tankStatus.rbegin()->second.get());
            }

            continue;
        }
        
        delta.height = (finishStatus.height - startStatus.height) / static_cast<float>(stepCount);
        if (std::abs(delta.height) > DELTA_MAX.height)
        {
            stepCount = (finishStatus.height - startStatus.height) / DELTA_MAX.height;
            const int stepAddiotion = stepCount - startStepCount; //количество добавляемыш шагов
            if (std::distance(finish, _tankStatus.end()) < stepAddiotion)
            {
                finishStatus = *(std::next(finish, stepAddiotion)->second.get());
            }
            else
            {
                finishStatus = *(_tankStatus.rbegin()->second.get());
            }

            continue;
        }
 
        delta.temp = (finishStatus.temp - startStatus.temp) / static_cast<float>(stepCount);
        if (std::abs(delta.temp) > DELTA_MAX.temp)
        {
            stepCount = (finishStatus.temp - startStatus.temp) / DELTA_MAX.temp;
            const int stepAddiotion = stepCount - startStepCount; //количество добавляемыш шагов
            if (std::distance(finish, _tankStatus.end()) < stepAddiotion)
            {
                finishStatus = *(std::next(finish, stepAddiotion)->second.get());
            }
            else
            {
                finishStatus = *(_tankStatus.rbegin()->second.get());
            }

            continue;
        }
        
        delta.volume = (finishStatus.volume - startStatus.volume) / static_cast<float>(stepCount);
        if (std::abs(delta.volume) > DELTA_MAX.volume)
        {
            stepCount = (finishStatus.volume - startStatus.volume) / DELTA_MAX.volume;
            const int stepAddiotion = stepCount - startStepCount; //количество добавляемыш шагов
            if (std::distance(finish, _tankStatus.end()) < stepAddiotion)
            {
                finishStatus = *(std::next(finish, stepAddiotion)->second.get());
            }
            else
            {
                finishStatus = *(_tankStatus.rbegin()->second.get());
            }

            continue;
        }
        
        delta.mass = (finishStatus.mass - startStatus.mass) / static_cast<float>(stepCount);
        if (std::abs(delta.mass) > DELTA_MAX.mass)
        {
            stepCount = (finishStatus.mass - startStatus.mass) / DELTA_MAX.mass;
            const int stepAddiotion = stepCount - startStepCount; //количество добавляемыш шагов
            if (std::distance(finish, _tankStatus.end()) < stepAddiotion)
            {
                finishStatus = *(std::next(finish, stepAddiotion)->second.get());
            }
            else
            {
                finishStatus = *(_tankStatus.rbegin()->second.get());
            }

            continue;
        }
        
        //если мы дошли до сюда, то все ок и дельты расчитаны правильно
        break;
    }    
    while (true);

    auto tmp = Status(startStatus);
    tmp.flag = static_cast<quint8>(AdditionFlag::CALCULATE);

    auto tankStatus_it = start;
    for (auto i = 0; i < stepCount; ++i)
    {
        tmp.density += delta.density;
        tmp.height += delta.height;
        tmp.temp += delta.temp;
        tmp.volume += delta.volume;
        tmp.mass += delta.mass;

        time = time.addSecs(60);
        
        //пока вставляем в свободное место - просто вставляем
        if (time < timeFinish.addSecs(-45))
        {
            tankStatus_it = _tankStatus.insert({time.addMSecs(rg->bounded(-1000, +1000)) , std::make_unique<Status>(tmp)}).first;
        }
        else
        {
            insertTankStatus(time, tmp);
        }

        //добавить рандомный +/-
        addRandom(tankStatus_it);
    }

    return tankStatus_it;
}

Tank::TankStatusIterator Tank::addStatusIntake(Tank::TankStatusIterator start, Tank::TankStatusIterator finish)
{
    Q_ASSERT(start != _tankStatus.end());
    Q_ASSERT(finish != _tankStatus.end());

    QDateTime time = start->first;  //время на текущем шаге

    //расчитываем сколько шагов нужно для подъема уровня
    const auto startStatus = *(start->second.get());
    const auto finishStatus = *(finish->second.get());

    const int intakeStepCount = static_cast<int>((finish->second->height - start->second->height) / (DELTA_MAX_INTAKE.height * 0.95));
    
    Status tmp(startStatus);
    tmp.flag = static_cast<quint8>(AdditionFlag::CALCULATE);

    //вставляем полочку в начале
    auto tankStatus_it = start;
    for (auto i = 0; i < MIN_STEP_COUNT_START_INTAKE; ++i)
    {
        time = time.addSecs(60);

        tankStatus_it = insertTankStatus(time, tmp);

        //добавить рандомный +/-
        addRandom(tankStatus_it);
    }

    //вставляем подъем
    Delta delta;
    delta.density = (finishStatus.density - startStatus.density) / static_cast<float>(intakeStepCount);
    delta.temp = (finishStatus.temp - startStatus.temp) / static_cast<float>(intakeStepCount);
    delta.volume = (finishStatus.volume - startStatus.volume) / static_cast<float>(intakeStepCount);
    delta.mass = (finishStatus.mass - startStatus.mass) / static_cast<float>(intakeStepCount);

    for (auto i = 0; i < intakeStepCount; ++i)
    {
        tmp.density += delta.density;
        tmp.height += delta.height;
        tmp.temp += delta.temp;
        tmp.volume += delta.volume;
        tmp.mass += delta.mass;

        time = time.addSecs(60);

        tankStatus_it = insertTankStatus(time, tmp);

        //добавить рандомный +/-
        addRandom(tankStatus_it);
    }

    //вставляем полочку в конце
    for (auto i = 0; i < MIN_STEP_COUNT_FINISH_INTAKE; ++i)
    {
        time = time.addSecs(60);

        tankStatus_it = insertTankStatus(time, tmp);

        //добавить рандомный +/-
        addRandom(tankStatus_it);
    }

    return tankStatus_it;
}

void Tank::addStatusEnd(const QDateTime& finish)
{
    Q_ASSERT(!_tankStatus.empty());

    auto time = _tankStatus.rbegin()->first;  //время на текущем шаге

    auto tmp = *(_tankStatus.rbegin()->second.get());
    tmp.flag = static_cast<quint8>(AdditionFlag::CALCULATE);

    while (time.secsTo(finish) >= 30)
    {
        time = time.addSecs(60);

        auto tankStatus_it = _tankStatus.insert({time.addMSecs(rg->bounded(-1000, +1000)) , std::make_unique<Status>(tmp)}).first;

        //добавить рандомный +/-
        addRandom(tankStatus_it);
    }
}

Tank::TankStatusIterator Tank::addStatusStart(const QDateTime &start)
{
    Q_ASSERT(!_tankStatus.empty());

    auto tankStatus_it = _tankStatus.begin();

    auto time = start;  //время на текущем шаге

    auto tmp = *(_tankStatus.begin()->second.get());
    tmp.flag = static_cast<quint8>(AdditionFlag::CALCULATE);

    while (time.secsTo(_tankStatus.begin()->first) >= 30)
    {
        time = time.addSecs(60);

        tankStatus_it = _tankStatus.insert({time.addMSecs(rg->bounded(-1000, +1000)) , std::make_unique<Status>(tmp)}).first;

        //добавить рандомный +/-
        addRandom(tankStatus_it);
    }

    return tankStatus_it;
}

void Tank::addRandom(Tank::TankStatusIterator it)
{
    it->second->density += round(static_cast<float>(rg->bounded(-100, +100)) / 200.0) * 0.1f;
    it->second->height  += round(static_cast<float>(rg->bounded(-100, +100)) / 200.0) * 1.0f;
    it->second->temp    += round(static_cast<float>(rg->bounded(-100, +100)) / 200.0) * 0.1f;
    it->second->volume  += round(static_cast<float>(rg->bounded(-100, +100)) / 200.0) * 10.0f;
    it->second->mass    += round(static_cast<float>(rg->bounded(-100, +100)) / 200.0) * 10.0f;
}

void Tank::saveToNitDB()
{
    Q_ASSERT(_db.isOpen());

    auto lastSaveDateTime = _tankConfig.lastSave;
    for (auto tankStatus_it = _tankStatus.upper_bound(_tankConfig.lastSave);
         tankStatus_it != _tankStatus.end() && tankStatus_it->first <= QDateTime::currentDateTime();
         ++tankStatus_it)
    {
        if (!_tankConfig.serviceMode)
        {
            //Сохряняем в БД НИТа
            const auto queryText =
                QString("INSERT INTO [%1].[dbo].[TanksStatus] ([DateTime], [AZSCode], [TankNumber], [Product], [Height], [Volume], [Temp], [Density], [Mass]) "
                        "VALUES (CAST('%2' AS DATETIME2), '%3', %4, '%5', %6, %7, %8, %9, %10)")
                    .arg(_tankConfig.dbNitName)
                    .arg(tankStatus_it->first.toString("yyyy-MM-dd hh:mm:ss.zzz"))
                    .arg(_tankConfig.AZSCode)
                    .arg(_tankConfig.tankNumber)
                    .arg(_tankConfig.product)
                    .arg(tankStatus_it->second->height / 10.0, 0, 'f', 1)
                    .arg(tankStatus_it->second->volume, 0, 'f', 0)
                    .arg(tankStatus_it->second->temp, 0, 'f', 1)
                    .arg(tankStatus_it->second->density, 0, 'f', 1)
                    .arg(tankStatus_it->second->mass, 0, 'f', 0);

            dbQueryExecute(queryText);
        }

        //Сохраняем в нашу БД
        const auto queryText =
            QString("INSERT INTO [TanksStatus] ([DateTime], [LoadDateTime], [AZSCode], [TankNumber], [Product], [Height], [Volume], [Temp], [Density], [Mass], [Flag]) "
                           "VALUES (CAST('%2' AS DATETIME2), CAST('%3' AS DATETIME2),'%4', %5, '%6', %7, %8, %9, %10, %11, %12)")
                .arg(_tankConfig.dbNitName)
                .arg(tankStatus_it->first.toString("yyyy-MM-dd hh:mm:ss.zzz"))
                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
                .arg(_tankConfig.AZSCode)
                .arg(_tankConfig.tankNumber)
                .arg(_tankConfig.product)
                .arg(tankStatus_it->second->height / 10.0, 0, 'f', 1)
                .arg(tankStatus_it->second->volume, 0, 'f', 0)
                .arg(tankStatus_it->second->temp, 0, 'f', 1)
                .arg(tankStatus_it->second->density, 0, 'f', 1)
                .arg(tankStatus_it->second->mass, 0, 'f', 0)
                .arg(static_cast<quint8>(tankStatus_it->second->flag));

        dbQueryExecute(queryText);

        lastSaveDateTime = tankStatus_it->first;
    }

    dbCommit();

    _tankConfig.lastSave = lastSaveDateTime; //обновляем дату пoследей отправленной записи
}

void Tank::saveIntake()
{
    auto start_it = Tank::getStartIntake();
    if (start_it == _tankStatus.end())
    {
        return;
    }

    auto finish_it = Tank::getFinishedIntake();
    if (finish_it == _tankStatus.end())
    {
        return;
    }

    //если есть начало и конец-то регистрируем приход
    if (!_tankConfig.serviceMode)
    {
        //сохраняем приход в БД НИТа
        const auto queryText =
            QString("INSERT INTO [%1].[dbo].[AddProduct] "
                    "([DateTime], [AZSCode], [TankNumber], [Product], [StartDateTime], [FinishedDateTime], [Height], [Volume], [Temp], [Density], [Mass]) "
                    "VALUES (CAST('%1' AS DATETIME2), '%2', %3, '%4', CAST('%5' AS DATETIME2), CAST('%6' AS DATETIME2), %7, %8, %9, %10, %11, %12)")
                .arg(_tankConfig.dbNitName)
                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
                .arg(_tankConfig.AZSCode)
                .arg(_tankConfig.tankNumber)
                .arg(_tankConfig.product)
                .arg(start_it->first.toString("yyyy-MM-dd hh:mm:ss.zzz"))
                .arg(finish_it->first.toString("yyyy-MM-dd hh:mm:ss.zzz"))
                .arg(finish_it->second->height / 10.0, 0, 'f', 1)
                .arg(finish_it->second->volume, 0, 'f', 0)
                .arg(finish_it->second->temp, 0, 'f', 1)
                .arg(finish_it->second->density, 0, 'f', 1)
                .arg(finish_it->second->mass, 0, 'f', 0);

        dbQueryExecute(queryText);
    }

    quint8 flag = static_cast<quint8>(AdditionFlag::UNKNOWN);
    for (auto it = start_it; it != finish_it; it ++)
    {
         flag = flag | static_cast<quint8>(it->second->flag);
    }

    //вставляем в нашу таблицу
    auto queryText =
            QString("INSERT INTO [TanksIntake] "
                    "([DateTime], [AZSCode], [TankNumber], [Product],   "
                    " [StartDateTime] ,[StartHeight], [StartVolume], [StartTemp], [StartDensity], [StartMass], "
                    " [FinishDateTime], [FinishHeight], [FinishStartVolume], [FinishtTemp], [FinishDensity], [FinishMass]) "
                    "VALUES (CAST('%1' AS DATETIME2), '%2', %3, '%4', "
                    " CAST('%5' AS DATETIME2), %6, %7, %8, %9, %10, %11, "
                    " CAST('%12' AS DATETIME2), %13, %14, %15, %16, %17, %18, "
                    " %19)")
                .arg(_tankConfig.dbNitName)
                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
                .arg(_tankConfig.AZSCode)
                .arg(_tankConfig.tankNumber)
                .arg(_tankConfig.product)
                .arg(start_it->first.toString("yyyy-MM-dd hh:mm:ss.zzz"))
                .arg(start_it->second->height / 10.0, 0, 'f', 1)
                .arg(start_it->second->volume, 0, 'f', 0)
                .arg(start_it->second->temp, 0, 'f', 1)
                .arg(start_it->second->density, 0, 'f', 1)
                .arg(start_it->second->mass, 0, 'f', 0)
                .arg(finish_it->first.toString("yyyy-MM-dd hh:mm:ss.zzz"))
                .arg(finish_it->second->height / 10.0, 0, 'f', 1)
                .arg(finish_it->second->volume, 0, 'f', 0)
                .arg(finish_it->second->temp, 0, 'f', 1)
                .arg(finish_it->second->density, 0, 'f', 1)
                .arg(finish_it->second->mass, 0, 'f', 0)
                .arg(flag);

    dbQueryExecute(queryText);

    //обновляем информацию о найденном сливае
    queryText = QString("UPDATE [TanksInfo] SET "
                        "[LastIntakeDateTime] = CAST('%1' AS DATETIME2) "
                        "WHERE ([AZSCode] = '%2') AND ([TankNumber] =%2) ")
                    .arg(finish_it->first.toString("yyyy-MM-dd hh:mm:ss.zzz"))
                    .arg(_tankConfig.AZSCode)
                    .arg(_tankConfig.tankNumber);

    dbQueryExecute(queryText);

    _tankConfig.lastIntake = finish_it->first;
}

Tank::TankStatusIterator Tank::getStartIntake()
{
    auto startTankStatus_it = _tankStatus.upper_bound(_tankConfig.lastIntake);
    if (std::distance(startTankStatus_it, _tankStatus.end()) <= MIN_STEP_COUNT_START_INTAKE)
    {
        return _tankStatus.end();
    }

    for (auto finishTankStatus_it = std::next(startTankStatus_it, MIN_STEP_COUNT_START_INTAKE);
         finishTankStatus_it != _tankStatus.end();
         ++finishTankStatus_it)
    {
        if (finishTankStatus_it->second->volume - startTankStatus_it->second->volume >= DELTA_INTAKE_VOLUME)
        {
            return finishTankStatus_it;
        }

        ++startTankStatus_it;
    }
    return _tankStatus.end();
}

Tank::Tank::TankStatusIterator Tank::getFinishedIntake()
{
    auto startTankStatus_it = _tankStatus.upper_bound(_tankConfig.lastIntake);
    if (std::distance(startTankStatus_it, _tankStatus.end()) <= MIN_STEP_COUNT_FINISH_INTAKE)
    {
        return _tankStatus.end();
    }

    for (auto finishTankStatus_it = std::next(startTankStatus_it, MIN_STEP_COUNT_FINISH_INTAKE);
         finishTankStatus_it != _tankStatus.end();
         ++finishTankStatus_it)
    {
        if (finishTankStatus_it->second->volume - startTankStatus_it->second->volume <= 0.0)
        {
            return startTankStatus_it;
        }

        ++startTankStatus_it;
    }
    return _tankStatus.end();
}

Tank::TankStatusIterator Tank::insertTankStatus(const QDateTime dateTime, const Status &status)
{
    Tank::TankStatusIterator result = _tankStatus.end();

    //Ищем ближайшую соотвествующую запись
    auto upperBoundTankStatus_it = _tankStatus.upper_bound(dateTime);
    if (upperBoundTankStatus_it == _tankStatus.end())
    {
        result = _tankStatus.insert({dateTime.addMSecs(rg->bounded(-1000, +1000)) , std::make_unique<Status>(status)}).first;
    }
    else
    {
        if (std::abs(upperBoundTankStatus_it->first.secsTo(dateTime)) < 45)
        {
            auto tmp = std::make_unique<Status>(status);
            tmp->flag = tmp->flag | static_cast<quint8>(AdditionFlag::CORRECTED);
            upperBoundTankStatus_it->second.swap(tmp);
            result = upperBoundTankStatus_it;
        }
        else
        {
            result = _tankStatus.insert({dateTime.addMSecs(rg->bounded(-1000, +1000)) , std::make_unique<Status>(status)}).first;
        }
    }

    if (result != _tankStatus.end() && result->first < _tankConfig.lastCheck)
    {
        _tankConfig.lastCheck = result->first;
    }

    return result;
}

