#include "QMutex"
#include "QMutexLocker"
#include "QSqlError"
#include "QSqlQuery"

#include "tank.h"

using namespace LevelGaugeService;
using namespace Common;

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

static Tank::AdditionFlag IntToAdditionFlag(quint8 status)
{
    switch (status)
    {
    case static_cast<quint8>(Tank::AdditionFlag::MEASUMENTS): return Tank::AdditionFlag::MEASUMENTS;
    case static_cast<quint8>(Tank::AdditionFlag::CALCULATE): return Tank::AdditionFlag::CALCULATE;
    case static_cast<quint8>(Tank::AdditionFlag::MANUAL): return Tank::AdditionFlag::MANUAL;
    }
    return Tank::AdditionFlag::UNKNOW;
}


static Tank::AdditionFlag AdditionFlagIntToAdditionFlag(quint8 status)
{
    switch (status)
    {
    case static_cast<quint8>(Tank::AdditionFlag::MEASUMENTS): return Tank::AdditionFlag::MEASUMENTS;
    case static_cast<quint8>(Tank::AdditionFlag::CALCULATE): return Tank::AdditionFlag::CALCULATE;
    case static_cast<quint8>(Tank::AdditionFlag::MANUAL): return Tank::AdditionFlag::MANUAL;
    }
    return Tank::AdditionFlag::UNKNOW;
}

static int connectionNumber()
{
    static QMutex mutex;
    static int connectionNumberValue = 0;

    QMutexLocker locker(&mutex);

    return ++connectionNumberValue;
}

Tank::Tank(const TankConfig& tankConfig, QObject *parent)
    : QObject{parent}
    , _cnf(TConfig::config())
    , _loger(Common::TDBLoger::DBLoger())
    , _tankConfig(tankConfig)
    , _dbConnectionName(QString("DB_%1").arg(connectionNumber()))
{
    Q_CHECK_PTR(_cnf);
    Q_CHECK_PTR(_loger);

    Q_ASSERT(!_tankConfig.AZSCode.isEmpty());
    Q_ASSERT(!_tankConfig.dbNitName.isEmpty());
    Q_ASSERT(_tankConfig.tankNumber != 0);
    Q_ASSERT(_tankConfig.diametr < 1.0);
    Q_ASSERT(_tankConfig.volume < 1.0);


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

QString Tank::errorString()
{
    auto res = _errorString;
    _errorString.clear();

    return res;
}

void Tank::start()
{
    connectToDB();

    //Подключаем логер
    QObject::connect(_loger, SIGNAL(sendLogMsg(int, const QString&)), SLOT(sendLogMsg(int, const QString&)), Qt::QueuedConnection);

    initFromSave();

   //создаем основной рабочий таймер
    _timer = new QTimer();
    _timer->setSingleShot(false);

    QObject::connect(_timer, SIGNAL(timeout()), SLOT(calculate()));

    _timer->start(60000);

    calculate();
}

void Tank::connectToDB()
{
    //настраиваем подключение БД
    const auto& dbConnectionInfo = _cnf->dbConnectionInfo();
    _db = QSqlDatabase::addDatabase(dbConnectionInfo.db_Driver, _dbConnectionName);
    _db.setDatabaseName(dbConnectionInfo.db_DBName);
    _db.setUserName(dbConnectionInfo.db_UserName);
    _db.setPassword(dbConnectionInfo.db_Password);
    _db.setConnectOptions(dbConnectionInfo.db_ConnectOptions);
    _db.setPort(dbConnectionInfo.db_Port);
    _db.setHostName(dbConnectionInfo.db_Host);

    //подключаемся к БД
    if ((_db.isOpen()) || (!_db.open()))
    {
        _errorString = QString("Cannot connect to database. Error: %1").arg(_db.lastError().text());

        return;
    };
}

void Tank::initFromSave()
{
    //т.к. приоритетное значение имеет сохранненные измерения - то сначала загружаем их
    _db.transaction();
    QSqlQuery query(_db);
    query.setForwardOnly(true);

    QString queryText =
        QString("SELECT [TankNumber], [DateTime], [Volume], [Mass], [Density], [Height], [Temp], [Flag] "
                "FROM [TanksStatus] "
                "WHERE [AZSCode] = '%1' AND [TankNumber] = %2 AND [DateTime] >= CAST('%3' AS DATETIME2) "
                "ORDER BY [DateTime] ")
            .arg(_tankConfig.AZSCode)
            .arg(_tankConfig.tankNumber)
            .arg(_tankConfig.lastIntake.toString("yyyy-MM-dd hh:mm:ss.zzz"));

    if (!query.exec(queryText))
    {
        errorDBQuery(_db, query);
    }

    //сохраняем
    bool added = false;
    while (query.next())
    {
        Status tmp;
        tmp.density = query.value("Density").toFloat();
        tmp.height = query.value("Height").toFloat() * 10; //переводим высоту обратно в мм
        tmp.mass = query.value("Mass").toFloat();
        tmp.temp = query.value("Temp").toFloat();
        tmp.volume = query.value("Volume").toFloat();
        tmp.flag = IntToAdditionFlag(query.value("Flag").toUInt());

        _tankStatus.insert(query.value("DateTime").toDateTime(), std::move(tmp));

        added = true;
    }

    if (added)
    {
        _tankConfig.lastSave = _tankStatus.lastKey();
        _tankConfig.lastMeasuments = _tankStatus.lastKey();
        _tankConfig.lastCheck = _tankStatus.lastKey();
    }
    else
    {
        _tankConfig.lastSave = _tankConfig.lastIntake;
        _tankConfig.lastMeasuments = _tankConfig.lastIntake;
        _tankConfig.lastCheck = _tankConfig.lastIntake;
    }

    DBCommit(_db);
}

void Tank::loadFromMeasument()
{
    _db.transaction();
    QSqlQuery query(_db);
    query.setForwardOnly(true);

    QString queryText =
        QString("SELECT [ID], [DateTime], [Density], [Height], [Volume], [Mass], [Temp] "
                "FROM [TanksMeasument] "
                "WHERE [AZSCode] = '%1' AND [TankNumber] = %2 AND [DateTime] > CAST('%3' AS DATETIME2) "
                "ORDER BY [DateTime] ")
            .arg(_tankConfig.AZSCode)
            .arg(_tankConfig.tankNumber)
            .arg(_tankConfig.lastMeasuments.toString("yyyy-MM-dd hh:mm:ss.zzz"));

    if (!query.exec(queryText))
    {
        errorDBQuery(_db, query);
    }

    while (query.next())
    {
        Status tmp;
        tmp.density = query.value("Density").toFloat();
        tmp.height = query.value("Height").toFloat();
        tmp.mass = query.value("Mass").toFloat();
        tmp.temp = query.value("Temp").toFloat();
        tmp.volume = query.value("Volume").toFloat();
        tmp.flag = AdditionFlag::MEASUMENTS;

        _tankStatus.insert(query.value("DateTime").toDateTime(), std::move(tmp));

        _tankConfig.lastMeasuments = query.value("DateTime").toDateTime();
    }

    DBCommit(_db);
}

void Tank::calculate()
{
    //загружаем данные из БД с измерениями
    loadFromMeasument();

    if (_tankStatus.empty())
    {
        return;
    }

    //проверяем и добавляем недостающие записи статусов
    checkStatus();

    saveToNitDB();

    saveIntake();
}

void Tank::checkStatus()
{
    Q_ASSERT(!_tankStatus.empty());

    //гарантируеться что _tankStatus не пустой

    //находим последний проверенный статус
    auto tankStatusStartCheck_it = _tankStatus.lowerBound(_tankConfig.lastCheck);

    //если это не самый конец вектора - то проверяем все имеющися
    if (tankStatusStartCheck_it != _tankStatus.end())
    {
        //проверяем что доступны все необходимые записи в середине имяющихся записей
        for (auto tankStatus_it = std::next(tankStatusStartCheck_it); tankStatus_it != _tankStatus.end(); ++tankStatus_it)
        {
            const auto tankStatus_prev_it = std::prev(tankStatus_it);

            const auto tankStatusPrevDateTime = tankStatus_prev_it.key();
            const auto tankStatusDateTime = tankStatus_it.key();

            const quint64 deltaTime = tankStatusPrevDateTime.secsTo(tankStatusDateTime); //в сек
            //сравниваем время между соседнями записами. Если разрыв более 120 секунд - необходимо добавить недостающие записи
            if (deltaTime > 120)
            {
                //Возможные варианты:
                //1. уровень уменьшился или остался прежний или увеличился менее допустимого(температурное расширение) - обычный режим работы АЗС
                //2. уровень вырос больше допустимого - был приход

                const auto deltaHeight = tankStatus_it.value().height - tankStatus_prev_it.value().height;
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

    //если с конца осутствует более 11 минут - то скорее всего обрыв связи с АЗС, поэтому добавляем
    //данные вручную

    const QDateTime currentDateTime = QDateTime::currentDateTime().addSecs(_tankConfig.timeShift); //Текущее время на АЗС
    if (_tankStatus.lastKey().secsTo(currentDateTime) > 660)
    {
        addStatusEnd(currentDateTime);
    }
}

Tank::TTankStatus::Iterator Tank::addStatusRange(Tank::TTankStatus::Iterator start, Tank::TTankStatus::Iterator finish)
{
    Q_ASSERT(start != _tankStatus.end());
    Q_ASSERT(finish != _tankStatus.end());

    QDateTime time = start.key();  //время на текущем шаге

    //вычисляем дельту (значения нужно именно скопироватть, т.к. итераторы могут стать недействительными)
    const auto startStatus = start.value();
    const auto finishStatus = finish.value();

    int stepCount = static_cast<int>(time.secsTo(finish.key()) / 60.0) - 1 ; //количество шагов которое у нас есть для вставки
    if (stepCount < 1) 
    {
        return start;
    }
    
    //Вычисляем необходимые дельты. Дельта слишком большая - приделься перезаписать часть уже имеющихся данных

    Delta delta;
    bool controlDelta = false;
    
    do
    {
        delta.density = (finishStatus.density - startStatus.density) / static_cast<float>(stepCount);
        if (std::abs(delta.density) > DELTA_MAX.density)
        {
            stepCount = (finishStatus.density - startStatus.density) / DELTA_MAX.density;
            
            continue;
        }
        
        delta.height = (finishStatus.height - startStatus.height) / static_cast<float>(stepCount);
        if (std::abs(delta.height) > DELTA_MAX.height)
        {
            stepCount = (finishStatus.height - startStatus.height) / DELTA_MAX.height;
            
            continue;
        }
 
        delta.temp = (finishStatus.temp - startStatus.temp) / static_cast<float>(stepCount);
        if (std::abs(delta.temp) > DELTA_MAX.temp)
        {
            stepCount = (finishStatus.temp - startStatus.temp) / DELTA_MAX.temp;
            
            continue;
        }
        
        delta.volume = (finishStatus.volume - startStatus.volume) / static_cast<float>(stepCount);
        if (std::abs(delta.volume) > DELTA_MAX.volume)
        {
            stepCount = (finishStatus.volume - startStatus.volume) / DELTA_MAX.volume;
            
            continue;
        }
        
        delta.mass = (finishStatus.mass - startStatus.mass) / static_cast<float>(stepCount);
        if (std::abs(delta.mass) > DELTA_MAX.mass)
        {
            stepCount = (finishStatus.mass - startStatus.mass) / DELTA_MAX.mass;
            
            continue;
        }
        
        //если мы дошли до сюда, то все ок и дельты расчитаны правильно
        controlDelta = true;
    }    
    while (!controlDelta);

    Status tmp(startStatus);
    tmp.flag = AdditionFlag::CALCULATE;

    auto tankStatus_it = start;
    for (auto i = 0; i < stepCount; ++i)
    {
        tmp.density += delta.density;
        tmp.height += delta.height;
        tmp.temp += delta.temp;
        tmp.volume += delta.volume;
        tmp.mass += delta.mass;

        time = time.addSecs(60);
        
        auto lowerBoundTankStatus_it = _tankStatus.lowerBound(time);
        
        if (lowerBoundTankStatus_it.key().secsTo(time) < 45) 
        {
            *lowerBoundTankStatus_it = tmp;
            tankStatus_it = lowerBoundTankStatus_it;
        }
        else 
        {
            tankStatus_it = _tankStatus.insert(time.addMSecs(rg->bounded(-1000, +1000)) , tmp);
        }

        //добавить рандомный +/-
        addRandom(tankStatus_it);
    }

    return tankStatus_it;
}

Tank::TTankStatus::Iterator Tank::addStatusIntake(Tank::TTankStatus::Iterator start, Tank::TTankStatus::Iterator finish)
{
    Q_ASSERT(start != _tankStatus.end());
    Q_ASSERT(finish != _tankStatus.end());

    QDateTime time = start.key();  //время на текущем шаге

    //расчитываем сколько шагов нужно для подъема уровня
    const auto startStatus = start.value();
    const auto finishStatus = finish.value();

    const int intakeStepCount = static_cast<int>((finish.value().height - start.value().height) / (DELTA_MAX_INTAKE.height * 0.95));
    
    Status tmp(startStatus);
    tmp.flag = AdditionFlag::CALCULATE;

    //вставляем полочку в начале
    auto tankStatus_it = start;
    for (auto i = 0; i < 5; ++i)
    {
        time = time.addSecs(60);

        auto lowerBoundTankStatus_it = _tankStatus.lowerBound(time);

        if (lowerBoundTankStatus_it.key().secsTo(time) < 45)
        {
            *lowerBoundTankStatus_it = tmp;
            tankStatus_it = lowerBoundTankStatus_it;
        }
        else
        {
            tankStatus_it = _tankStatus.insert(time.addMSecs(rg->bounded(-1000, +1000)) , tmp);
        }

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

        auto lowerBoundTankStatus_it = _tankStatus.lowerBound(time);

        if (lowerBoundTankStatus_it.key().secsTo(time) < 45)
        {
            *lowerBoundTankStatus_it = tmp;
            tankStatus_it = lowerBoundTankStatus_it;
        }
        else
        {
            tankStatus_it = _tankStatus.insert(time.addMSecs(rg->bounded(-1000, +1000)) , tmp);
        }

        //добавить рандомный +/-
        addRandom(tankStatus_it);
    }

    //вставляем полочку в конце
    for (auto i = 0; i < 10; ++i)
    {
        time = time.addSecs(60);

        auto lowerBoundTankStatus_it = _tankStatus.lowerBound(time);

        if (lowerBoundTankStatus_it.key().secsTo(time) < 45)
        {
            *lowerBoundTankStatus_it = tmp;
            tankStatus_it = lowerBoundTankStatus_it;
        }
        else
        {
            tankStatus_it = _tankStatus.insert(time.addMSecs(rg->bounded(-1000, +1000)) , tmp);
        }

        //добавить рандомный +/-
        addRandom(tankStatus_it);
    }

    return tankStatus_it;
}

void Tank::addStatusEnd(const QDateTime& finish)
{
    Q_ASSERT(!_tankStatus.empty());

    QDateTime time = _tankStatus.lastKey();  //время на текущем шаге

    Status tmp = _tankStatus.last();
    tmp.flag = AdditionFlag::CALCULATE;

    while (time.secsTo(finish) >= 30)
    {
        time = time.addSecs(60);

        auto tankStatus_it = _tankStatus.insert(time.addMSecs(rg->bounded(-1000, +1000)) , tmp);

        //добавить рандомный +/-
        addRandom(tankStatus_it);
    }
}

void Tank::addRandom(Tank::TTankStatus::Iterator it)
{
    it->density += round(static_cast<float>(rg->bounded(-100, +100)) / 200.0) * 0.1;
    it->height  += round(static_cast<float>(rg->bounded(-100, +100)) / 200.0) * 1;
    it->temp    += round(static_cast<float>(rg->bounded(-100, +100)) / 200.0) * 0.1;
    it->volume  += round(static_cast<float>(rg->bounded(-100, +100)) / 200.0) * 10;
    it->mass    += round(static_cast<float>(rg->bounded(-100, +100)) / 200.0) * 10;
}

void Tank::saveToNitDB()
{
    Q_ASSERT(_db.isOpen());

    _db.transaction();
    QSqlQuery query(_db);

    for (auto tankStatus_it = _tankStatus.upperBound(_tankConfig.lastSave);
         tankStatus_it != _tankStatus.end() && tankStatus_it.key() <= QDateTime::currentDateTime();
         ++tankStatus_it)
    {
        //Сохряняем в БД НИТа
        QString queryText =
            QString("INSERT INTO [%1].[dbo].[TanksStatus] ([DateTime], [AZSCode], [TankNumber], [Product], [Height], [Volume], [Temp], [Density], [Mass]) "
                    "VALUES (CAST('%2' AS DATETIME2), '%3', %4, '%5', %6, %7, %8, %9, %10)")
                .arg(_tankConfig.dbNitName)
                .arg(tankStatus_it.key().toString("yyyy-MM-dd hh:mm:ss.zzz"))
                .arg(_tankConfig.AZSCode)
                .arg(_tankConfig.tankNumber)
                .arg(_tankConfig.product)
                .arg(tankStatus_it->height / 10.0, 0, 'f', 1)
                .arg(tankStatus_it->volume, 0, 'f', 0)
                .arg(tankStatus_it->temp, 0, 'f', 1)
                .arg(tankStatus_it->density, 0, 'f', 1)
                .arg(tankStatus_it->mass, 0, 'f', 0);

        //qDebug() << QueryText

        if (!query.exec(queryText))
        {
            errorDBQuery(_db, query);
        }

        //Сохраняем в нашу БД
        queryText =
            QString("INSERT INTO [TanksStatus] ([DateTime], [LoadDateTime], [AZSCode], [TankNumber], [Product], [Height], [Volume], [Temp], [Density], [Mass], [Flag]) "
                           "VALUES (CAST('%2' AS DATETIME2), CAST('%3' AS DATETIME2),'%4', %5, '%6', %7, %8, %9, %10, %11, %12)")
                .arg(_tankConfig.dbNitName)
                .arg(tankStatus_it.key().toString("yyyy-MM-dd hh:mm:ss.zzz"))
                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
                .arg(_tankConfig.AZSCode)
                .arg(_tankConfig.tankNumber)
                .arg(_tankConfig.product)
                .arg(tankStatus_it->height / 10.0, 0, 'f', 1)
                .arg(tankStatus_it->volume, 0, 'f', 0)
                .arg(tankStatus_it->temp, 0, 'f', 1)
                .arg(tankStatus_it->density, 0, 'f', 1)
                .arg(tankStatus_it->mass, 0, 'f', 0)
                .arg(static_cast<quint8>(tankStatus_it->flag));

          //qDebug() << QueryText

        if (!query.exec(queryText))
        {
            errorDBQuery(_db, query);
        }
    }

    DBCommit(_db);

    _tankConfig.lastSave = _tankStatus.lastKey(); //обновляем дату пoследей отправленной записи
}

void Tank::saveIntake()
{
    auto start_it = Tank::getStartIntake();
    if (start_it != _tankStatus.end())
    {
        return;
    }

    auto finish_it = Tank::getFinishedIntake();
    if (finish_it != _tankStatus.end())
    {
        return;
    }

    //если есть начало и конец-то регистрируем приход
    _db.transaction();
    QSqlQuery query(_db);

    //сохраняем приход в БД НИТа
    QString queryText =
        QString("INSERT INTO [%1].[dbo].[AddProduct] "
                "([DateTime], [AZSCode], [TankNumber], [Product], [StartDateTime], [FinishedDateTime], [Height], [Volume], [Temp], [Density], [Mass])"
                "VALUES (CAST('%1' AS DATETIME2), '%2', %3, '%4', CAST('%5' AS DATETIME2), CAST('%6' AS DATETIME2), %7, %8, %9, %10, %11, %12)")
            .arg(_tankConfig.dbNitName)
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
            .arg(_tankConfig.AZSCode)
            .arg(_tankConfig.tankNumber)
            .arg(_tankConfig.product)
            .arg(start_it.key().toString("yyyy-MM-dd hh:mm:ss.zzz"))
            .arg(finish_it.key().toString("yyyy-MM-dd hh:mm:ss.zzz"))
            .arg(finish_it->height / 10.0, 0, 'f', 1)
            .arg(finish_it->volume, 0, 'f', 0)
            .arg(finish_it->temp, 0, 'f', 1)
            .arg(finish_it->density, 0, 'f', 1)
            .arg(finish_it->mass, 0, 'f', 0);

    // qDebug() << QueryText;

    if (!query.exec(queryText))
    {
       errorDBQuery(_db, query);
    }

    quint8 flag = static_cast<quint8>(AdditionFlag::UNKNOW);
    for (auto it = start_it; it != finish_it; it ++)
    {
         flag = flag | static_cast<quint8>(it->flag);
    }

    //вставляем в нашу таблицу
    queryText =
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
                .arg(start_it.key().toString("yyyy-MM-dd hh:mm:ss.zzz"))
                .arg(start_it->height / 10.0, 0, 'f', 1)
                .arg(start_it->volume, 0, 'f', 0)
                .arg(start_it->temp, 0, 'f', 1)
                .arg(start_it->density, 0, 'f', 1)
                .arg(start_it->mass, 0, 'f', 0)
                .arg(finish_it.key().toString("yyyy-MM-dd hh:mm:ss.zzz"))
                .arg(finish_it->height / 10.0, 0, 'f', 1)
                .arg(finish_it->volume, 0, 'f', 0)
                .arg(finish_it->temp, 0, 'f', 1)
                .arg(finish_it->density, 0, 'f', 1)
                .arg(finish_it->mass, 0, 'f', 0)
                .arg(flag);

    // qDebug() << QueryText;

    if (!query.exec(queryText))
    {
       errorDBQuery(_db, query);
    }

    //обновляем информацию о найденном сливае
    queryText = QString("UPDATE [TanksInfo] SET "
                        "[LastIntakeDateTime] = CAST('%1' AS DATETIME2) "
                        "WHERE ([AZSCode] = '%2') AND ([TankNumber] =%2) ")
                    .arg(finish_it.key().toString("yyyy-MM-dd hh:mm:ss.zzz"))
                    .arg(_tankConfig.AZSCode)
                    .arg(_tankConfig.tankNumber);

    // qDebug() << QueryText;

    if (!query.exec(queryText))
    {
       errorDBQuery(_db, query);
    }

    DBCommit(_db);

    _tankConfig.lastIntake = finish_it.key();
}

Tank::TTankStatus::Iterator Tank::getStartIntake()
{
    static const int MIN_STEP_COUNT = 5;

    auto startTankStatus_it = _tankStatus.upperBound(_tankConfig.lastIntake);
    if (std::distance(startTankStatus_it, _tankStatus.end()) <= MIN_STEP_COUNT)
    {
        return _tankStatus.end();
    }

    for (auto finishTankStatus_it = std::next(startTankStatus_it, MIN_STEP_COUNT);
         finishTankStatus_it != _tankStatus.end();
         ++finishTankStatus_it)
    {
        if (finishTankStatus_it.value().volume - startTankStatus_it->volume >= DELTA_INTAKE_VOLUME)
        {
            return finishTankStatus_it;
        }

        ++startTankStatus_it;
    }
    return _tankStatus.end();
}

Tank::TTankStatus::Iterator Tank::getFinishedIntake()
{
    static const int MIN_STEP_COUNT = 10;

    auto startTankStatus_it = _tankStatus.upperBound(_tankConfig.lastIntake);
    if (std::distance(startTankStatus_it, _tankStatus.end()) <= MIN_STEP_COUNT)
    {
        return _tankStatus.end();
    }

    for (auto finishTankStatus_it = std::next(startTankStatus_it, MIN_STEP_COUNT);
         finishTankStatus_it != _tankStatus.end();
         ++finishTankStatus_it)
    {
        if (finishTankStatus_it.value().volume - startTankStatus_it->volume <= 0.0)
        {
            return startTankStatus_it;
        }

        ++startTankStatus_it;
    }
    return _tankStatus.end();
}

