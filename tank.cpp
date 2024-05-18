//STL
#include <algorithm>

//My
#include "tank.h"

using namespace LevelGaugeService;
using namespace Common;

static const int MIN_STEP_COUNT_START_INTAKE = 5;   //время полочки в начале приемки топлива
static const int MIN_STEP_COUNT_FINISH_INTAKE = 10; //время полочки по окончанию приемки топлива
static constexpr int MIN_TIME_START_INTAKE = 60 * MIN_STEP_COUNT_START_INTAKE;   //время полочки в начале приемки топлива, сек
static constexpr int MIN_TIME_FINISH_INTAKE = 60 * MIN_STEP_COUNT_FINISH_INTAKE; //время полочки по окончанию приемки топлива, сек

static const int MIN_STEP_COUNT_START_PUMPING_OUT = 5;   //время полочки в начале отпуска топлива
static const int MIN_STEP_COUNT_FINISH_PUMPING_OUT = 10; //время полочки по окончанию отпуска топлива

static const int SKIP_TIME = 50;

static constexpr int TIME_TO_SAVE = std::max(MIN_TIME_START_INTAKE, MIN_STEP_COUNT_START_PUMPING_OUT * 60) + 60;

static constexpr int AZS_CONNECTION_TIMEOUT = 60 * 10;     //Таймаут обрыва связи с уровнемером, сек
static const float FLOAT_EPSILON = 0.0000001f;

Tank::Tank(const LevelGaugeService::TankConfig* tankConfig, TankStatusesList&& tankSavedStatuses, QObject *parent /* = nullptr) */)
    : QObject{parent}
    , _tankConfig(tankConfig)
    , _rg(QRandomGenerator::global())
{
    Q_CHECK_PTR(_rg);
    Q_CHECK_PTR(_tankConfig);

    _lastSendToSaveDateTime = _tankConfig->lastSave();
    _lastPumpingOut = _tankConfig->lastSave();

    for (auto& tankStatus: tankSavedStatuses)
    {
        addStatus(std::move(tankStatus));
    }

    if (!_tankStatuses.empty())
    {
        const auto& lastTankStatus = _tankStatuses.crbegin()->second;
        if (lastTankStatus->status() == TankConfig::Status::INTAKE)
        {
            _isIntake = lastTankStatus->dateTime();
        }
        else if (lastTankStatus->status() == TankConfig::Status::PUMPING_OUT)
        {
            _isPumpingOut = lastTankStatus->dateTime();
        }
    }
}

Tank::~Tank()
{
    stop();
}

void Tank::start()
{
    Q_ASSERT(_addEndTimer == nullptr);

    _addEndTimer = new QTimer;

    QObject::connect(_addEndTimer, SIGNAL(timeout()), SLOT(addStatusEnd()));



    _saveToDBTimer = new QTimer;

    QObject::connect(_saveToDBTimer, SIGNAL(timeout()), SLOT(sendNewStatusesToSave()));

    _saveToDBTimer->start(60000);
}

void Tank::stop()
{
    delete _addEndTimer;
    _addEndTimer = nullptr;

    delete _saveToDBTimer;
    _saveToDBTimer = nullptr;

    emit finished();
}

void Tank::newStatuses(const TankID &id, const TankStatusesList &tankStatuses)
{
    if (id != _tankConfig->tankId())
    {
        return;
    }

    if (!_addEndTimer->isActive())
    {
        _addEndTimer->start(60000);
    }

    if (tankStatuses.empty())
    {
        return;
    }

    const auto lastStatusDateTime = _tankStatuses.empty() ? QDateTime::currentDateTime().addMonths(-1) : _tankStatuses.crbegin()->first.addSecs(SKIP_TIME);

    //копируем только те статусы, которые имеют метку времени позже уже имеющихся
    TankStatusesList tankStatusesSorted(tankStatuses.size());
    auto tankStatusesSorted_it = std::copy_if(tankStatuses.begin(), tankStatuses.end(), tankStatusesSorted.begin(),
        [&lastStatusDateTime](const auto& status)
        {
            return status.getTankStatusData().dateTime > lastStatusDateTime;
        });
    tankStatusesSorted.erase(tankStatusesSorted_it, tankStatusesSorted.end());

    //сортируем статусы по возрастанию метки времени
    tankStatusesSorted.sort(
        [](const auto& status1, const auto& status2)
        {
            return status1.getTankStatusData().dateTime < status2.getTankStatusData().dateTime;
        });

    for (auto& tankStatus:  tankStatusesSorted)
    {
        checkLimits(&tankStatus);
    }

    if (tankStatusesSorted.empty())
    {
        return;
    }

    addStatuses(tankStatusesSorted);

    findIntake();
    findPumpingOut();
}

void Tank::addStatuses(const TankStatusesList &tankStatuses)
{
    for (const auto& tankStatus: tankStatuses)
    {
        if (_tankStatuses.empty())
        {
            addStatusesRange(tankStatus);

            continue;
        }
        if (tankStatus.dateTime() <= _tankStatuses.crbegin()->first)
        {
            emit sendLogMsg(_tankConfig->tankId(), TDBLoger::MSG_CODE::WARNING_CODE, QString("Status with date: %1 will be skipped becuse there is already a status with a later date: %2")
                            .arg(tankStatus.dateTime().toString(DATETIME_FORMAT))
                            .arg(_tankStatuses.crbegin()->first.toString(DATETIME_FORMAT)));

            continue;
        }

        Q_ASSERT(!_tankStatuses.empty());
        const auto lastHeight = _tankStatuses.crbegin()->second->height();
        const auto currentHeight = tankStatus.height();

        TankStatus statusForAdd(tankStatus);

        checkLimits(&statusForAdd);

        if ((currentHeight - lastHeight) > _tankConfig->deltaIntakeHeight())
        {
            addStatusesIntake(statusForAdd);
        }
        else
        {
            addStatusesRange(statusForAdd);
        }
    }
}

void Tank::addStatus(const LevelGaugeService::TankStatus& tankStatus)
{
    TankStatus tmp(tankStatus);
    addStatus(std::move(tmp));
}

void Tank::addStatus(LevelGaugeService::TankStatus &&tankStatus)
{
    auto tankStatus_p = std::make_unique<TankStatus>(std::move(tankStatus));
    if (tankStatus_p->status() != TankConfig::Status::REPAIR)
    {
        if (_isIntake.has_value())
        {
            tankStatus_p->setStatus(TankConfig::Status::INTAKE);
        }
        else if (_isPumpingOut.has_value())
        {
            tankStatus_p->setStatus(TankConfig::Status::PUMPING_OUT);
        }
    }

    _tankStatuses.emplace(tankStatus.getTankStatusData().dateTime, std::move(tankStatus_p));
}


void Tank::checkLimits(LevelGaugeService::TankStatus* status) const
{
    const auto& limits = _tankConfig->limits();

    //Проверяем лимитные ограничения
    //density
    if (status->density() < limits.density.first)
    {
        status->setDensity(limits.density.first);
        status->setAdditionFlag(status->additionFlag() | static_cast<quint8>(TankStatus::AdditionFlag::CORRECTED));
    }
    else if (status->density() > limits.density.second)
    {
        status->setDensity(limits.density.second);
        status->setAdditionFlag(status->additionFlag() | static_cast<quint8>(TankStatus::AdditionFlag::CORRECTED));
    }

    //height
    if (status->height() < limits.height.first)
    {
        status->setHeight(limits.height.first);
        status->setAdditionFlag(status->additionFlag() | static_cast<quint8>(TankStatus::AdditionFlag::CORRECTED));
    }
    else if (status->height() > limits.height.second)
    {
        status->setHeight(limits.height.second);
        status->setAdditionFlag(status->additionFlag() | static_cast<quint8>(TankStatus::AdditionFlag::CORRECTED));
    }

    //mass
    if (status->mass() < limits.mass.first)
    {
        status->setMass(limits.mass.first);
        status->setAdditionFlag(status->additionFlag() | static_cast<quint8>(TankStatus::AdditionFlag::CORRECTED));
    }
    else if (status->mass() > limits.mass.second)
    {
        status->setMass(limits.mass.second);
        status->setAdditionFlag(status->additionFlag() | static_cast<quint8>(TankStatus::AdditionFlag::CORRECTED));
    }

    //volume
    if (status->volume() < limits.volume.first)
    {
        status->setVolume(limits.volume.first);
        status->setAdditionFlag(status->additionFlag() | static_cast<quint8>(TankStatus::AdditionFlag::CORRECTED));
    }
    else if (status->volume() > limits.volume.second)
    {
        status->setVolume(limits.volume.second);
        status->setAdditionFlag(status->additionFlag() | static_cast<quint8>(TankStatus::AdditionFlag::CORRECTED));
    }

    //temp
    if (status->temp() < limits.temp.first)
    {
        status->setTemp(limits.temp.first);
        status->setAdditionFlag(status->additionFlag() | static_cast<quint8>(TankStatus::AdditionFlag::CORRECTED));
    }
    else if (status->temp() > limits.temp.second)
    {
        status->setTemp(limits.temp.second);
        status->setAdditionFlag(status->additionFlag() | static_cast<quint8>(TankStatus::AdditionFlag::CORRECTED));
    }
}

void Tank::sendNewStatusesToSave()
{
    const auto startSave_it = _tankStatuses.upper_bound(_lastSendToSaveDateTime);
    if (startSave_it == _tankStatuses.end())
    {
        return;
    }

    TankStatusesList statusesForSave;
    for(auto tankStatus_it = startSave_it;
        (tankStatus_it != _tankStatuses.end() && (QDateTime::currentDateTime().secsTo(tankStatus_it->first) < -TIME_TO_SAVE));
        ++tankStatus_it)
    {
        statusesForSave.push_back(*tankStatus_it->second);
        _lastSendToSaveDateTime = std::max(_lastSendToSaveDateTime, tankStatus_it->first);
    }

    if (!statusesForSave.empty())
    {
        emit calculateStatuses(_tankConfig->tankId(), statusesForSave);
    }

    clearTankStatuses();
}

void Tank::addStatusesRange(const LevelGaugeService::TankStatus& tankStatus)
{
    //если никаких данных нет - то выходим
    if (_tankStatuses.empty())
    {
        addStatus(tankStatus);

        return;
    }

    const TankStatus lastTankStatus(*(_tankStatuses.crbegin()->second.get()));

    //время на текущем шаге
    auto time = lastTankStatus.dateTime();

    //количество шагов которое у нас есть для вставки
    int stepCount = static_cast<int>(static_cast<double>(time.secsTo(tankStatus.dateTime())) / 60.0);

    //вычисляем дельту
    TankConfig::Delta delta;

    do
    {
        delta.height = (tankStatus.height() - lastTankStatus.height()) / static_cast<float>(stepCount + 1);
        delta.density = (tankStatus.density() - lastTankStatus.density()) / static_cast<float>(stepCount + 1);
        delta.temp = (tankStatus.temp() - lastTankStatus.temp()) / static_cast<float>(stepCount + 1);
        delta.volume = (tankStatus.volume() - lastTankStatus.volume()) / static_cast<float>(stepCount + 1);
        delta.mass = (tankStatus.mass() - lastTankStatus.mass()) / static_cast<float>(stepCount + 1);

        //Если дельты слишком большие - то увеличиваем количество необходимых шагов
        const auto& deltaMax = _tankConfig->deltaMax();
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

    auto tmp = TankStatus(lastTankStatus); //статус на текущем шаге
    tmp.setAdditionFlag(static_cast<quint8>(TankStatus::AdditionFlag::CALCULATE));
    tmp.setStatus(tankStatus.status());

    for (auto i = 0; i < stepCount; ++i)
    {
        time = time.addSecs(60);

        tmp.setDensity(tmp.density() + delta.density);
        tmp.setHeight(tmp.height() + delta.height);
        tmp.setTemp(tmp.temp() + delta.temp);
        tmp.setVolume(tmp.volume() + delta.volume);
        tmp.setMass(tmp.mass() + delta.mass);
        tmp.setDateTime(time);

        TankStatus statusForAdd(tmp);

        //добавить рандомный +/-
        addRandom(&statusForAdd);
        checkLimits(&statusForAdd);
        addStatus(statusForAdd);
    }

    if (stepCount > 1)
    {
        emit sendLogMsg(_tankConfig->tankId(), TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Added range. Start time: %1 Finish time: %2. Steps count: %3")
                        .arg(lastTankStatus.dateTime().toString(DATETIME_FORMAT))
                        .arg(time.toString(DATETIME_FORMAT))
                        .arg(stepCount));
    }

    time = time.addSecs(60);
    TankStatus statusForAdd(tankStatus);
    statusForAdd.setDateTime(time);
    addStatus(statusForAdd);
}

void Tank::addStatusesIntake(const TankStatus& tankStatus)
{
    //расчитываем сколько шагов нужно для подъема уровня
    const auto lastTankStatus = *(_tankStatuses.crbegin()->second.get());

    const int startStepCount = MIN_TIME_START_INTAKE / 60 + 1;
    const int intakeStepCount = static_cast<int>(tankStatus.height() - lastTankStatus.height()) / (_tankConfig->deltaIntake().height * 0.95);
    const int finishStepCount = MIN_TIME_FINISH_INTAKE / 60 + 1;

    //время на текущем шаге
    auto time = lastTankStatus.dateTime();

    //вставляем полочку в начале
    for (auto i = 0; i < startStepCount; ++i)
    {
        time = time.addSecs(60);

        TankStatus statusForAdd(lastTankStatus);
        statusForAdd.setDateTime(time);

        //добавить рандомный +/-
        addRandom(&statusForAdd);
        checkLimits(&statusForAdd);
        addStatus(statusForAdd);
    }

    //вставляем подъем
    auto tmp = lastTankStatus;
    tmp.setAdditionFlag(static_cast<quint8>(TankStatus::AdditionFlag::CALCULATE));
    tmp.setStatus(tankStatus.status() == TankConfig::Status::REPAIR ? TankConfig::Status::REPAIR : TankConfig::Status::INTAKE);

    TankConfig::Delta delta;
    delta.height = (tankStatus.height() - lastTankStatus.height()) / static_cast<float>(intakeStepCount + 1);
    delta.density = (tankStatus.density() - lastTankStatus.density()) / static_cast<float>(intakeStepCount + 1);
    delta.temp = (tankStatus.temp() - lastTankStatus.temp()) / static_cast<float>(intakeStepCount + 1);
    delta.volume = (tankStatus.volume() - lastTankStatus.volume()) / static_cast<float>(intakeStepCount + 1);
    delta.mass = (tankStatus.mass() - lastTankStatus.mass()) / static_cast<float>(intakeStepCount + 1);

    for (auto i = 0; i < intakeStepCount; ++i)
    {
        time = time.addSecs(60);

        tmp.setDensity(tmp.density() + delta.density);
        tmp.setHeight(tmp.height() + delta.height);
        tmp.setTemp(tmp.temp() + delta.temp);
        tmp.setVolume(tmp.volume() + delta.volume);
        tmp.setMass(tmp.mass() + delta.mass);
        tmp.setDateTime(time);

        TankStatus statusForAdd(tmp);

        //добавить рандомный +/-
        addRandom(&statusForAdd);
        checkLimits(&statusForAdd);
        addStatus(statusForAdd);
    }

    //вставляем полочку в конце
    for (auto i = 0; i < finishStepCount; ++i)
    {
        time = time.addSecs(60);

        TankStatus statusForAdd(tankStatus);
        statusForAdd.setDateTime(time);

        //добавить рандомный +/-
        addRandom(&statusForAdd);
        checkLimits(&statusForAdd);
        addStatus(statusForAdd);
    }

    emit sendLogMsg(_tankConfig->tankId(), TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Added intake. Start time: %1 Finish time: %2 Delta height: %3. Steps count: %4")
                    .arg(lastTankStatus.dateTime().toString(DATETIME_FORMAT))
                    .arg(time.toString(DATETIME_FORMAT))
                    .arg(tankStatus.height() - lastTankStatus.height())
                    .arg(startStepCount + intakeStepCount + finishStepCount));
}

void Tank::addStatusEnd()
{
    if (_tankStatuses.empty())
    {
        emit sendLogMsg(_tankConfig->tankId(), TDBLoger::MSG_CODE::WARNING_CODE, "There is no status for the tank");

        return;
    }

    auto time = _tankStatuses.crbegin()->first;  //время последнего статуса
    if (time.secsTo(QDateTime::currentDateTime()) < AZS_CONNECTION_TIMEOUT)
    {
        return;
    }

    const auto startTime = time;

    auto lastStatus_it = std::find_if(_tankStatuses.crbegin(), _tankStatuses.crend(),
        [](const auto& status)
        {
            return status.second->additionFlag() != static_cast<quint8>(TankStatus::AdditionFlag::UNKNOWN);
        });

    auto lastStatus = lastStatus_it != _tankStatuses.crend() ? *(lastStatus_it->second.get()) : *(_tankStatuses.rbegin()->second.get());
    lastStatus.setAdditionFlag(static_cast<quint8>(TankStatus::AdditionFlag::UNKNOWN));

    quint64 addedCount = 0;
    while (time.secsTo(QDateTime::currentDateTime()) >= AZS_CONNECTION_TIMEOUT)
    {
        time = time.addSecs(60);

        TankStatus tmp(lastStatus);
        tmp.setDateTime(time);

        //добавить рандомный +/-
        addRandom(&tmp);
        checkLimits(&tmp);
        addStatus(tmp);

        ++addedCount;
    }

    emit sendLogMsg(_tankConfig->tankId(), TDBLoger::MSG_CODE::WARNING_CODE, QString("Added statuses to end. Start: %1. Finish: %2. Count: %3")
                    .arg(startTime.toString(DATETIME_FORMAT))
                    .arg(_tankStatuses.crbegin()->first.toString(DATETIME_FORMAT))
                    .arg(addedCount));
}

void Tank::addRandom(LevelGaugeService::TankStatus* tankStatus) const
{
    tankStatus->setDateTime(tankStatus->dateTime().addMSecs(_rg->bounded(-1000, +1000)));

    const auto density = tankStatus->density() + round(static_cast<float>(_rg->bounded(-100, +100)) / 180.0f) * 0.1f;
    tankStatus->setDensity(density);

    const auto height = tankStatus->height() + round(static_cast<float>(_rg->bounded(-100, +100)) / 180.0f) * 1.0f;
    tankStatus->setHeight(height > 1.0 ? height : 1.0);

    const auto temp = tankStatus->temp() + round(static_cast<float>(_rg->bounded(-100, +100)) / 180.0f) * 0.1f;
    tankStatus->setTemp(temp);

    const auto volume = tankStatus->volume() + round(static_cast<float>(_rg->bounded(-100, +100)) / 180.0f) * 10.0f;
    tankStatus->setVolume(volume > 10.0 ? volume : 10.0);

    const auto mass = tankStatus->mass() + round(static_cast<float>(_rg->bounded(-100, +100)) / 180.0f) * 10.0;
    tankStatus->setMass(mass > 10.0 ? mass : 10.0);
}

void Tank::clearTankStatuses()
{
    auto lastTankStatuses_it = _tankStatuses.end();
    for (auto tankStatuses_it = _tankStatuses.begin(); tankStatuses_it != _tankStatuses.end(); ++tankStatuses_it)
    {
        if (tankStatuses_it->first < _tankConfig->lastIntake())
        {
            lastTankStatuses_it = tankStatuses_it;
        }
    }

    if (lastTankStatuses_it != _tankStatuses.end())
    {
        _tankStatuses.erase(_tankStatuses.begin(), lastTankStatuses_it);
    }
}

Tank::TankStatusesIterator Tank::getStartIntake()
{
    auto startTankStatus_it = _tankStatuses.upper_bound(_tankConfig->lastIntake());
    if (std::distance(startTankStatus_it, _tankStatuses.end()) <= MIN_STEP_COUNT_START_INTAKE)
    {
        return _tankStatuses.end();
    }

    for (auto finishTankStatus_it = std::next(startTankStatus_it, MIN_STEP_COUNT_START_INTAKE);
         finishTankStatus_it != _tankStatuses.end();
         ++finishTankStatus_it)
    {
        if (finishTankStatus_it->second->height() - startTankStatus_it->second->height() >= _tankConfig->deltaIntakeHeight())
        {
            return std::prev(finishTankStatus_it, MIN_STEP_COUNT_START_INTAKE);
        }

        ++startTankStatus_it;
    }
    return _tankStatuses.end();
}

Tank::TankStatusesIterator Tank::getFinishedIntake()
{
    auto startTankStatus_it = _tankStatuses.upper_bound(_isIntake.value());
    if (std::distance(startTankStatus_it, _tankStatuses.end()) <= MIN_STEP_COUNT_FINISH_INTAKE)
    {
        return _tankStatuses.end();
    }

    for (auto finishTankStatus_it = std::next(startTankStatus_it, MIN_STEP_COUNT_FINISH_INTAKE);
         finishTankStatus_it != _tankStatuses.end();
         ++finishTankStatus_it)
    {
        if (finishTankStatus_it->second->height() - startTankStatus_it->second->height() <= FLOAT_EPSILON)
        {
            return finishTankStatus_it;
        }

        ++startTankStatus_it;
    }
    return _tankStatuses.end();
}

void Tank::findIntake()
{
    if (_tankStatuses.size() <  MIN_STEP_COUNT_START_INTAKE + MIN_STEP_COUNT_FINISH_INTAKE)
    {
        return;
    }

    Tank::TankStatusesIterator start_it = _tankStatuses.end();
    if (!_isIntake.has_value())
    {
        start_it = Tank::getStartIntake();
        if (start_it == _tankStatuses.end())
        {
            return;
        }

        _isIntake = start_it->first;
        for (auto tankStatuses_it = start_it; tankStatuses_it != _tankStatuses.end(); ++tankStatuses_it)
        {
            tankStatuses_it->second->setStatus(tankStatuses_it->second->status() == TankConfig::Status::REPAIR ? TankConfig::Status::REPAIR : TankConfig::Status::INTAKE);
        }

        emit sendLogMsg(_tankConfig->tankId(), TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Find started intake at: %1").arg(_isIntake.value().toString(DATETIME_FORMAT)));
    }

    if (!_isIntake.has_value())
    {
        return;
    }

    auto finish_it = Tank::getFinishedIntake();
    if (finish_it == _tankStatuses.end())
    {
        return;
    }

    _lastPumpingOut = finish_it->first;

    start_it = _tankStatuses.find(_isIntake.value());
    Q_ASSERT(start_it != _tankStatuses.end());

    emit sendLogMsg(_tankConfig->tankId(), TDBLoger::MSG_CODE::INFORMATION_CODE,
                    QString("Find finished intake. Start: %1. Finish: %2. Delta volume: %3, mass: %4, height: %5, density: %6, temp: %7")
                        .arg(_isIntake.value().toString(DATETIME_FORMAT))
                        .arg(finish_it->first.toString(DATETIME_FORMAT))
                        .arg(finish_it->second->volume() - start_it->second->volume())
                        .arg(finish_it->second->mass() - start_it->second->mass())
                        .arg(finish_it->second->height() - start_it->second->height())
                        .arg(finish_it->second->density() - start_it->second->density())
                        .arg(finish_it->second->temp() - start_it->second->temp())
                    );

    IntakesList intakesList;
    Intake tmp(_tankConfig->tankId(), *(start_it->second.get()), *(finish_it->second.get()));

    intakesList.emplace_back(std::move(tmp));

    emit calculateIntakes(_tankConfig->tankId(), intakesList);

    _isIntake.reset();
}

Tank::TankStatusesIterator Tank::getStartPumpingOut()
{
    auto startTankStatus_it = _tankStatuses.upper_bound(_lastPumpingOut);
    if (std::distance(startTankStatus_it, _tankStatuses.end()) <= MIN_STEP_COUNT_START_PUMPING_OUT)
    {
        return _tankStatuses.end();
    }

    for (auto finishTankStatus_it = std::next(startTankStatus_it, MIN_STEP_COUNT_START_PUMPING_OUT);
         finishTankStatus_it != _tankStatuses.end();
         ++finishTankStatus_it)
    {
        if (finishTankStatus_it->second->height() - startTankStatus_it->second->height() <= -_tankConfig->deltaPumpingOutHeight())
        {
            return std::prev(finishTankStatus_it, MIN_STEP_COUNT_START_PUMPING_OUT);
        }

        ++startTankStatus_it;
    }

    return _tankStatuses.end();
}

Tank::TankStatusesIterator Tank::getFinishedPumpingOut()
{
    if (!_isPumpingOut.has_value())
    {
        return _tankStatuses.end();
    }

    auto startTankStatus_it = _tankStatuses.upper_bound(_isPumpingOut.value());
    if (std::distance(startTankStatus_it, _tankStatuses.end()) <= MIN_STEP_COUNT_FINISH_PUMPING_OUT)
    {
        return _tankStatuses.end();
    }

    for (auto finishTankStatus_it = std::next(startTankStatus_it, MIN_STEP_COUNT_FINISH_PUMPING_OUT);
         finishTankStatus_it != _tankStatuses.end();
         ++finishTankStatus_it)
    {
        if (finishTankStatus_it->second->height() - startTankStatus_it->second->height() <= -FLOAT_EPSILON)
        {
            return finishTankStatus_it;
        }

        ++startTankStatus_it;
    }
    return _tankStatuses.end();
}

void Tank::findPumpingOut()
{
    if (_tankStatuses.size() <  MIN_STEP_COUNT_START_PUMPING_OUT + MIN_STEP_COUNT_FINISH_PUMPING_OUT)
    {
        return;
    }

    auto start_it = _tankStatuses.end();
    if (!_isPumpingOut.has_value())
    {
        start_it = Tank::getStartPumpingOut();
        if (start_it == _tankStatuses.end())
        {
            return;
        }

        _isPumpingOut = start_it->first;
        for (auto tankStatuses_it = start_it; tankStatuses_it != _tankStatuses.end(); ++tankStatuses_it)
        {
            tankStatuses_it->second->setStatus(tankStatuses_it->second->status() == TankConfig::Status::REPAIR ? TankConfig::Status::REPAIR : TankConfig::Status::PUMPING_OUT);
        }

        emit sendLogMsg(_tankConfig->tankId(), TDBLoger::MSG_CODE::INFORMATION_CODE, QString("Find started pumping out at: %1").arg(_isPumpingOut.value().toString(DATETIME_FORMAT)));
    }

    if (!_isPumpingOut.has_value())
    {
        return;
    }

    auto finish_it = Tank::getFinishedPumpingOut();
    if (finish_it == _tankStatuses.end())
    {
        return;
    }

    _lastPumpingOut = finish_it->first;

    start_it = _tankStatuses.find(_isPumpingOut.value());
    Q_ASSERT(start_it != _tankStatuses.end());

    emit sendLogMsg(_tankConfig->tankId(), TDBLoger::MSG_CODE::INFORMATION_CODE,
                    QString("Find finished pumping out. Start: %1. Finish: %2. Delta volume: %3, mass: %4, height: %5, density: %6, temp: %7")
                        .arg(_isPumpingOut.value().toString(DATETIME_FORMAT))
                        .arg(finish_it->first.toString(DATETIME_FORMAT))
                        .arg(finish_it->second->volume() - start_it->second->volume())
                        .arg(finish_it->second->mass() - start_it->second->mass())
                        .arg(finish_it->second->height() - start_it->second->height())
                        .arg(finish_it->second->density() - start_it->second->density())
                        .arg(finish_it->second->temp() - start_it->second->temp())
                    );

    _isPumpingOut.reset();
}
