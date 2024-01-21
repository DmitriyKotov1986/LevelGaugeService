//STL
#include <algorithm>

//Qt
#include <QMutex>
#include <QMutexLocker>

//My
#include "Common/common.h"
#include "commondefines.h"

#include "tank.h"

using namespace LevelGaugeService;
using namespace Common;

static const int MIN_TIME_START_INTAKE = 60 * 5;   //время полочки в начале приемки топлива, сек
static const int MIN_TIME_FINISH_INTAKE = 60 * 10; //время полочки по окончанию приемки топлива, сек
static const int STUGLE_TIME = 60 * 20;            //Время отстоя нефтепродукта после приемки, сек
static const int AZS_CONNECTION_TIMEOUT = 60 * 11;     //Таймаут обрыва связи с уровнемером, сек

static int connectionNumber()
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
    stop();
}

void Tank::start()
{
    if (!connectToDB(_db, _dbConnectionInfo, _dbConnectionName))
    {
        emit errorOccurred(connectDBErrorString(_db));

        return;
    };

    loadTankConfig();
    makeLimits();
    initFromSave();

    //создаем основной рабочий таймер
    _timer = new QTimer();
    _timer->setSingleShot(false);

    QObject::connect(_timer, SIGNAL(timeout()), SLOT(calculate()));

    _timer->start(60000 + (rg->bounded(2000) - 1000));

    calculate();
}

void Tank::stop()
{
    delete _timer;

    if (_db.isOpen())
    {
        _db.close();
    }

    QSqlDatabase::removeDatabase(_dbConnectionName);

    emit finished();
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

    emit errorOccurred(executeDBErrorString(_db, query));

    _db.rollback();
}

void Tank::dbCommit()
{
    Q_ASSERT(_db.isOpen());

    if (!_db.commit())
    {
        emit errorOccurred(commitDBErrorString(_db));

        _db.rollback();
    }
}

void Tank::loadTankConfig()
{

}



void Tank::initFromSave()
{
       const auto timeLoad = std::max(MIN_TIME_START_INTAKE, STUGLE_TIME) * 2;
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
            .arg(_tankConfig.id.levelGaugeCode)
            .arg(_tankConfig.id.tankNumber)
            .arg(_tankConfig.lastMeasuments.toString(DATETIME_FORMAT));

    dbQueryExecute(query, queryText);

    while (query.next())
    {
        auto tmp = std::make_unique<TankStatuses::TankStatus>();
        tmp->density = query.value("Density").toFloat();
        tmp->height = query.value("Height").toFloat();
        tmp->mass = query.value("Mass").toFloat();
        tmp->temp = query.value("Temp").toFloat();
        tmp->volume = query.value("Volume").toFloat();
        tmp->flag = static_cast<quint8>(TankStatuses::AdditionFlag::MEASUMENTS);
        tmp->status = TankStatuses::Status::UNDEFINE;

        _tankTargetStatuses.add(query.value("DateTime").toDateTime().addSecs(-_tankConfig.timeShift), std::move(tmp));//время переводим во время сервера

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
    if (_tankTargetStatuses.isEmpty() && _tankResultStatuses.isEmpty())
    {
        emit errorOccurred("No data for work");

        return;
    }

    //проверяем и добавляем недостающие записи статусов
    makeResultStatus();
    statusDetect();
    sendNewStatuses();
    clearTankStatus();
}

void Tank::makeResultStatus()
{
    //если никаких данных нет - то выходим
    if (_tankTargetStatuses.isEmpty() && _tankResultStatuses.isEmpty())
    {
        return;
    }

    //если результирующая карта пустая - то записываем в нее первое целевое значение
    if (_tankResultStatuses.isEmpty())
    {
        _tankResultStatuses.add(_tankConfig.lastSave,
                                  std::make_unique<TankStatuses::TankStatus>(*(_tankTargetStatuses.begin()->second)));
    }

    TankStatuses::TankStatusesIterator tankTargetStatuses_it;
    do
    {
        //находим ближайшую цель
        const auto lastResultDateTime = _tankResultStatuses.rbegin()->first;
        tankTargetStatuses_it = _tankTargetStatuses.upperBound(lastResultDateTime);

        if (tankTargetStatuses_it != _tankTargetStatuses.end())
        {
            const auto& lastResultHeight = _tankResultStatuses.rbegin()->second->height;
            const auto& targetHeight = tankTargetStatuses_it->second->height;

            if ((targetHeight - lastResultHeight) > _tankConfig.deltaIntakeHeight)
            {
                addStatusIntake(*(tankTargetStatuses_it->second.get()));
            }
            else
            {
                addStatusRange(tankTargetStatuses_it->first, *(tankTargetStatuses_it->second.get()));
            }

            checkLimits(_tankResultStatuses.upperBound(lastResultDateTime));
        }
    }
    while (tankTargetStatuses_it != _tankTargetStatuses.end());

    //если с конца осутствует более AZS_CONNECTION_TIMEOUT секунд - то скорее всего обрыв связи с АЗС, поэтому добавляем
    //данные вручную
    if (_tankResultStatuses.rbegin()->first.secsTo(QDateTime::currentDateTime()) > AZS_CONNECTION_TIMEOUT)
    {
        addStatusEnd();
    }
}

void Tank::statusDetect()
{
    auto start_it = std::find_if(_tankResultStatuses.begin(), _tankResultStatuses.end(),
                        [](const auto& item)
                        {
                            return item.second.get()->status == TankStatuses::Status::UNDEFINE;
                        });

    if (start_it == _tankResultStatuses.end())
    {
        return;
    }


}

void Tank::checkLimits(TankStatuses::TankStatusesIterator start_it)
{
    const auto& limits = _tankConfig.limits;
    //Проверяем лимитные ограничения
    for (auto tankStatuses_it = start_it; tankStatuses_it != _tankResultStatuses.end(); ++tankStatuses_it)
    {
        auto status = tankStatuses_it->second.get();

        //density
        if (status->density < limits.density.first)
        {
            status->density = limits.density.first;
            status->flag = status->flag | static_cast<quint8>(TankStatuses::AdditionFlag::CORRECTED);
        }
        else if (status->density > limits.density.second)
        {
            status->density = limits.density.second;
            status->flag = status->flag | static_cast<quint8>(TankStatuses::AdditionFlag::CORRECTED);
        }

        //height
        if (status->height < limits.height.first)
        {
            status->height = limits.height.first;
            status->flag = status->flag | static_cast<quint8>(TankStatuses::AdditionFlag::CORRECTED);
        }
        else if (status->height > limits.height.second)
        {
            status->height = limits.height.second;
            status->flag = status->flag | static_cast<quint8>(TankStatuses::AdditionFlag::CORRECTED);
        }

        //mass
        if (status->mass < limits.mass.first)
        {
            status->mass = limits.mass.first;
            status->flag = status->flag | static_cast<quint8>(TankStatuses::AdditionFlag::CORRECTED);
        }
        else if (status->mass > limits.mass.second)
        {
            status->mass = limits.mass.second;
            status->flag = status->flag | static_cast<quint8>(TankStatuses::AdditionFlag::CORRECTED);
        }

        //volume
        if (status->volume < limits.volume.first)
        {
            status->volume = limits.volume.first;
            status->flag = status->flag | static_cast<quint8>(TankStatuses::AdditionFlag::CORRECTED);
        }
        else if (status->volume > limits.volume.second)
        {
            qDebug() << status->volume << limits.volume.second;
            status->volume = limits.volume.second;
            status->flag = status->flag | static_cast<quint8>(TankStatuses::AdditionFlag::CORRECTED);
        }

        //temp
        if (status->temp < limits.temp.first)
        {
            status->temp = limits.temp.first;
            status->flag = status->flag | static_cast<quint8>(TankStatuses::AdditionFlag::CORRECTED);
        }
        else if (status->temp > limits.temp.second)
        {
            status->temp = limits.temp.second;
            status->flag = status->flag | static_cast<quint8>(TankStatuses::AdditionFlag::CORRECTED);
        }
    }
}

void Tank::addStatusRange(const QDateTime& targetDateTime, const TankStatuses::TankStatus& targetStatus)
{
    Q_ASSERT(!_tankResultStatuses.isEmpty());

    const TankStatuses::TankStatus startTankStatus(*(_tankResultStatuses.rbegin()->second.get()));

    //время на текущем шаге
    auto time = _tankResultStatuses.rbegin()->first;

    //количество шагов которое у нас есть для вставки
    int stepCount = static_cast<int>(static_cast<double>(time.secsTo(targetDateTime)) / 60.0) - 1;

    //вычисляем дельту
    Delta delta;

    do
    {
        delta.height = (targetStatus.height - startTankStatus.height) / static_cast<float>(stepCount + 1);
        delta.density = (targetStatus.density - startTankStatus.density) / static_cast<float>(stepCount + 1);
        delta.temp = (targetStatus.temp - startTankStatus.temp) / static_cast<float>(stepCount + 1);
        delta.volume = (targetStatus.volume - startTankStatus.volume) / static_cast<float>(stepCount + 1);
        delta.mass = (targetStatus.mass - startTankStatus.mass) / static_cast<float>(stepCount + 1);

        //Если дельты слишком большие - то увеличиваем количество необходимых шагов
        const auto& deltaMax = _tankConfig.deltaMax;
        if ((std::abs(delta.height)  >= deltaMax.height)  ||
            (std::abs(delta.density) >= deltaMax.density) ||
            (std::abs(delta.temp)    >= deltaMax.temp)    ||
            (std::abs(delta.volume)  >= deltaMax.volume)  ||
            (std::abs(delta.mass)    >= deltaMax.mass ))

        {
            ++stepCount;
        }
        else
        {
            break;
        }
    }
    while (true);

    auto tmp = TankStatuses::TankStatus(startTankStatus); //статус на текущем шаге
    tmp.flag = static_cast<quint8>(TankStatuses::AdditionFlag::CALCULATE);
    tmp.status = TankStatuses::Status::UNDEFINE;

    for (auto i = 0; i < stepCount; ++i)
    {
        tmp.density += delta.density;
        tmp.height += delta.height;
        tmp.temp += delta.temp;
        tmp.volume += delta.volume;
        tmp.mass += delta.mass;

        time = time.addSecs(60);
        
        auto tankStatuses_it = _tankResultStatuses.add(time.addMSecs(rg->bounded(-1000, +1000)), std::make_unique<TankStatuses::TankStatus>(tmp));

        //добавить рандомный +/-
        addRandom(tankStatuses_it);
    }

    _tankResultStatuses.add(time.addSecs(60), std::make_unique<TankStatuses::TankStatus>(targetStatus));
}

void Tank::addStatusIntake(const TankStatuses::TankStatus& targetStatus)
{
    Q_ASSERT(!_tankResultStatuses.isEmpty());

    //время на текущем шаге
    auto time = _tankResultStatuses.rbegin()->first;

    //расчитываем сколько шагов нужно для подъема уровня
    const auto startStatus = *(_tankResultStatuses.rbegin()->second.get());

    const int startStepCount = MIN_TIME_START_INTAKE / 60 + 1;
    const int intakeStepCount = static_cast<int>((targetStatus.height - startStatus.height) / (_tankConfig.deltaIntake.height * 0.95));
    const int finishStepCount = MIN_TIME_FINISH_INTAKE / 60 + 1;

    auto tmp = startStatus;
    tmp.flag = static_cast<quint8>(TankStatuses::AdditionFlag::CALCULATE);
    tmp.status = TankStatuses::Status::UNDEFINE;

    //вставляем полочку в начале

    for (auto i = 0; i < startStepCount; ++i)
    {
        time = time.addSecs(60);

        auto tankStatus_it = _tankResultStatuses.add(time.addMSecs(rg->bounded(-1000, +1000)), std::make_unique<TankStatuses::TankStatus>(tmp));

        //добавить рандомный +/-
        addRandom(tankStatus_it);
    }

    //вставляем подъем
    Delta delta;
    delta.density = (targetStatus.density - startStatus.density) / static_cast<float>(intakeStepCount);
    delta.temp = (targetStatus.temp - startStatus.temp) / static_cast<float>(intakeStepCount);
    delta.volume = (targetStatus.volume - startStatus.volume) / static_cast<float>(intakeStepCount);
    delta.mass = (targetStatus.mass - startStatus.mass) / static_cast<float>(intakeStepCount);

    for (auto i = 0; i < intakeStepCount; ++i)
    {
        tmp.density += delta.density;
        tmp.height += delta.height;
        tmp.temp += delta.temp;
        tmp.volume += delta.volume;
        tmp.mass += delta.mass;

        time = time.addSecs(60);

        auto tankStatus_it = _tankResultStatuses.add(time.addMSecs(rg->bounded(-1000, +1000)), std::make_unique<TankStatuses::TankStatus>(tmp));

        //добавить рандомный +/-
        addRandom(tankStatus_it);
    }

    //вставляем полочку в конце
    for (auto i = 0; i < finishStepCount; ++i)
    {
        time = time.addSecs(60);

        auto tankStatus_it = _tankResultStatuses.add(time.addMSecs(rg->bounded(-1000, +1000)), std::make_unique<TankStatuses::TankStatus>(tmp));

        //добавить рандомный +/-
        addRandom(tankStatus_it);
    }
}

void Tank::addStatusEnd()
{
    Q_ASSERT(!_tankResultStatuses.isEmpty());

    auto time = _tankResultStatuses.rbegin()->first;  //время на текущем шаге
    auto tmp = *(_tankResultStatuses.rbegin()->second.get());
    tmp.flag = static_cast<quint8>(TankStatuses::AdditionFlag::CALCULATE);
    tmp.status = TankStatuses::Status::UNDEFINE;

    while (time.secsTo(QDateTime::currentDateTime()) >= AZS_CONNECTION_TIMEOUT)
    {
        time = time.addSecs(60);

        auto tankResultStatus_it = _tankResultStatuses.add(time.addMSecs(rg->bounded(-1000, +1000)) , std::make_unique<TankStatuses::TankStatus>(tmp));

        //добавить рандомный +/-
        addRandom(tankResultStatus_it);
    }
}

void Tank::addRandom(TankStatuses::TankStatusesIterator it)
{
    it->second->density += round(static_cast<float>(rg->bounded(-100, +100)) / 200.0f) * 0.1f;
    it->second->height  += round(static_cast<float>(rg->bounded(-100, +100)) / 200.0f) * 1.0f;
    it->second->temp    += round(static_cast<float>(rg->bounded(-100, +100)) / 200.0f) * 0.1f;
    it->second->volume  += round(static_cast<float>(rg->bounded(-100, +100)) / 200.0f) * 10.0f;
    it->second->mass    += round(static_cast<float>(rg->bounded(-100, +100)) / 200.0f) * 10.0f;
}

void Tank::sendNewStatuses()
{
    const auto start_it = _tankResultStatuses.upperBound(_tankConfig.lastSave);
    const auto end_it = _tankResultStatuses.lowerBound(QDateTime::currentDateTime().addSecs(-DETECT_STATUS_TIME));
    for (auto tankResultStatuses_it = start_it; tankResultStatuses_it != end_it; ++tankResultStatuses_it)
    {
        emit addStatusForSync(_tankConfig.id, tankResultStatuses_it->first.addSecs(DETECT_STATUS_TIME), *tankResultStatuses_it->second.get());
        _tankConfig.lastSave = std::max(_tankConfig.lastSave, tankResultStatuses_it->first);
    }
}

void Tank::clearTankStatus()
{
    _tankResultStatuses.clear(_tankConfig.lastSave.addSecs(-DETECT_STATUS_TIME));
    _tankTargetStatuses.clear(_tankConfig.lastSave.addSecs(-DETECT_STATUS_TIME));
}

