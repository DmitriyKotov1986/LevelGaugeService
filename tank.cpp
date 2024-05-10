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

static constexpr int STEP_COUNT_TO_SAVE = std::max(MIN_STEP_COUNT_START_INTAKE, MIN_STEP_COUNT_START_PUMPING_OUT) + 1;

static constexpr int AZS_CONNECTION_TIMEOUT = 60 * 11;     //Таймаут обрыва связи с уровнемером, сек
static const float FLOAT_EPSILON = 0.0000001f;

Tank::Tank(const LevelGaugeService::TankConfig* tankConfig, QObject *parent /* = nullptr) */)
    : QObject{parent}
    , _tankConfig(tankConfig)
    , _rg(QRandomGenerator::global())
{
    Q_CHECK_PTR(_rg);
    Q_CHECK_PTR(_tankConfig);

    _lastSendToSaveDateTime = _tankConfig->lastSave();
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

    _addEndTimer->start(60000);
}

void Tank::stop()
{
    delete _addEndTimer;
    _addEndTimer = nullptr;

    emit finished();
}

void Tank::newStatuses(const TankID &id, const TankStatusesList &tankStatuses)
{
    if (id != _tankConfig->tankId())
    {
        return;
    }

    if (tankStatuses.isEmpty())
    {
        return;
    }

    const auto lastStatusDateTime = _tankStatuses.empty() ? QDateTime::currentDateTime().addYears(-1) : _tankStatuses.crbegin()->first;

    //копируем тольк оте статусы, которые имеют метку времени позже уже имеющихся
    TankStatusesList tankStatusesSorted(tankStatuses.size());
    auto tankStatusesSorted_it = std::copy_if(tankStatuses.begin(), tankStatuses.end(), tankStatusesSorted.begin(),
        [&lastStatusDateTime](const auto& status)
        {
            return status.getTankStatusData().dateTime > lastStatusDateTime;
        });
    tankStatusesSorted.erase(tankStatusesSorted_it, tankStatusesSorted.end());

    //сортируем статусы по возрастанию метки времени
    std::sort(tankStatusesSorted.begin(), tankStatusesSorted.end(),
        [](const auto& status1, const auto& status2)
        {
            return status1.getTankStatusData().dateTime < status2.getTankStatusData().dateTime;
        });

    addStatuses(tankStatusesSorted);

    findIntake();
    findPumpingOut();

    sendNewStatusesToSave();
}

void Tank::addStatuses(const TankStatusesList &tankStatuses)
{
    for (const auto& tankStatus: tankStatuses)
    {
        if (!_tankStatuses.empty() && (tankStatus.dateTime() <= _tankStatuses.crbegin()->first))
        {
            continue;
        }

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
    auto tankStatus_p = std::make_unique<TankStatus>(tankStatus);
    if (_isIntake.has_value())
    {
        tankStatus_p->setStatus(TankConfig::Status::INTAKE);
    }
    else if (_isPumpingOut.has_value())
    {
        tankStatus_p->setStatus(TankConfig::Status::PUMPING_OUT);
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
    if (std::distance(startSave_it, _tankStatuses.end()) < STEP_COUNT_TO_SAVE)
    {
        return;
    }

    TankStatusesList statusesForSave;
    const auto finishedSave_it =  std::prev(_tankStatuses.end(), STEP_COUNT_TO_SAVE);
    for(auto tankStatus_it = startSave_it;  tankStatus_it != finishedSave_it; ++tankStatus_it)
    {
        statusesForSave.push_back(*tankStatus_it->second);
        _lastSendToSaveDateTime = std::max(_lastSendToSaveDateTime, tankStatus_it->first);
    }

    if (!statusesForSave.isEmpty())
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
    int stepCount = static_cast<int>(static_cast<double>(time.secsTo(tankStatus.dateTime())) / 60.0) - 1;

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

    addStatus(tankStatus);
}

void Tank::addStatusesIntake(const TankStatus& tankStatus)
{
    if (_tankStatuses.empty())
    {
        addStatus(tankStatus);

        return;
    }

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
    tmp.setStatus(TankConfig::Status::INTAKE);

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
}

void Tank::addStatusEnd()
{
    if (_tankStatuses.empty())
    {
        emit sendLogMsg(_tankConfig->tankId(), TDBLoger::MSG_CODE::WARNING_CODE, "There is no status for the tank");

        return;
    }

    auto time = _tankStatuses.crbegin()->first;  //время последнего статуса
    auto lastStatus = *(_tankStatuses.rbegin()->second.get());
    lastStatus.setAdditionFlag(static_cast<quint8>(TankStatus::AdditionFlag::UNKNOWN));

    while (time.secsTo(QDateTime::currentDateTime()) >= AZS_CONNECTION_TIMEOUT)
    {
        time = time.addSecs(60);

        TankStatus tmp(lastStatus);
        tmp.setDateTime(time);

        //добавить рандомный +/-
        addRandom(&tmp);

        checkLimits(&tmp);

        addStatus(tmp);
    }

    sendNewStatusesToSave();
}

void Tank::addRandom(LevelGaugeService::TankStatus* tankStatus) const
{
    tankStatus->setDateTime(tankStatus->dateTime().addMSecs(_rg->bounded(-1000, +1000)));
    tankStatus->setDensity(tankStatus->density() + round(static_cast<float>(_rg->bounded(-100, +100)) / 200.0f) * 0.1f);
    tankStatus->setHeight(tankStatus->height() + round(static_cast<float>(_rg->bounded(-100, +100)) / 200.0f) * 1.0f);
    tankStatus->setTemp(tankStatus->temp() + round(static_cast<float>(_rg->bounded(-100, +100)) / 200.0f) * 0.1f);
    tankStatus->setVolume(tankStatus->volume() + round(static_cast<float>(_rg->bounded(-100, +100)) / 200.0f) * 10.0f);
    tankStatus->setMass(tankStatus->mass() + round(static_cast<float>(_rg->bounded(-100, +100)) / 200.0f) * 10.0);
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
        if (finishTankStatus_it->second->volume() - startTankStatus_it->second->volume() >= _tankConfig->deltaIntakeHeight())
        {
            return finishTankStatus_it;
        }

        ++startTankStatus_it;
    }
    return _tankStatuses.end();
}

Tank::TankStatusesIterator Tank::getFinishedIntake()
{
    auto startTankStatus_it = _tankStatuses.upper_bound(_tankConfig->lastIntake());
    if (std::distance(startTankStatus_it, _tankStatuses.end()) <= MIN_STEP_COUNT_FINISH_INTAKE)
    {
        return _tankStatuses.end();
    }

    for (auto finishTankStatus_it = std::next(startTankStatus_it, MIN_STEP_COUNT_FINISH_INTAKE);
         finishTankStatus_it != _tankStatuses.end();
         ++finishTankStatus_it)
    {
        if (finishTankStatus_it->second->volume() - startTankStatus_it->second->volume() <= FLOAT_EPSILON)
        {
            return startTankStatus_it;
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
            tankStatuses_it->second->setStatus(TankConfig::Status::INTAKE);
        }
    }

    auto finish_it = Tank::getFinishedIntake();
    if (finish_it == _tankStatuses.end())
    {
        _isIntake.reset();

        return;
    }

    IntakesList intakesList;
    Intake tmp(_tankConfig->tankId(), *(start_it->second.get()), *(finish_it->second.get()));

    intakesList.emplace_back(std::move(tmp));

    emit calculateIntakes(_tankConfig->tankId(), intakesList);
}

Tank::TankStatusesIterator Tank::getStartPumpingOut()
{
    auto startTankStatus_it = _tankStatuses.upper_bound(_tankConfig->lastIntake());
    if (std::distance(startTankStatus_it, _tankStatuses.end()) <= MIN_STEP_COUNT_START_PUMPING_OUT)
    {
        return _tankStatuses.end();
    }

    for (auto finishTankStatus_it = std::next(startTankStatus_it, MIN_STEP_COUNT_START_PUMPING_OUT);
         finishTankStatus_it != _tankStatuses.end();
         ++finishTankStatus_it)
    {
        if (finishTankStatus_it->second->volume() - startTankStatus_it->second->volume() <= -_tankConfig->deltaPumpingOutHeight())
        {
            return finishTankStatus_it;
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
        if (finishTankStatus_it->second->volume() - startTankStatus_it->second->volume() <= -FLOAT_EPSILON)
        {
            return startTankStatus_it;
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
            tankStatuses_it->second->setStatus(TankConfig::Status::PUMPING_OUT);
        }
    }

    auto finish_it = Tank::getFinishedPumpingOut();
    if (finish_it == _tankStatuses.end())
    {
        _isPumpingOut.reset();

        return;
    }
}
