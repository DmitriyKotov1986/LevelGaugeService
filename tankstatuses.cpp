//STL
#include <algorithm>

//My
#include "tankstatuses.h"

LevelGaugeService::TankStatusesList::TankStatusesList(qsizetype size /* = 0 */)
    : _tankStatusesList(size)
{
}

LevelGaugeService::TankStatusesList::TankStatusListIterator LevelGaugeService::TankStatusesList::begin()
{
    return _tankStatusesList.begin();
}

LevelGaugeService::TankStatusesList::TankStatusListIterator LevelGaugeService::TankStatusesList::end()
{
    return _tankStatusesList.end();
}

LevelGaugeService::TankStatusesList::TankStatusListIteratorConst LevelGaugeService::TankStatusesList::begin() const
{
    return _tankStatusesList.cbegin();
}

LevelGaugeService::TankStatusesList::TankStatusListIteratorConst LevelGaugeService::TankStatusesList::end() const
{
    return _tankStatusesList.cend();
}

bool LevelGaugeService::TankStatusesList::empty() const
{
    return _tankStatusesList.empty();
}

qsizetype LevelGaugeService::TankStatusesList::size() const
{
    return _tankStatusesList.size();
}

void LevelGaugeService::TankStatusesList::emplace_back(TankStatus &&status)
{
    _tankStatusesList.emplace_back(std::move(status));
}

void LevelGaugeService::TankStatusesList::push_back(const TankStatus &status)
{
    _tankStatusesList.push_back(status);
}

qsizetype LevelGaugeService::TankStatusesList::filtered(const QDateTime &startDateTime)
{
    if (empty())
    {
        return 0;
    }

    auto oldSize = _tankStatusesList.size();

    //удаляем все статусы которы раньше имеющихся в наличии
    auto tankStatusesList_it =
        std::remove_if(_tankStatusesList.begin(), _tankStatusesList.end(),
            [&startDateTime](const auto& status)
            {
                return status.dateTime() <= startDateTime;
            });

    _tankStatusesList.erase(tankStatusesList_it, _tankStatusesList.end());

    //удаляем все статусы у которых разница меньше секунды
    tankStatusesList_it =
        std::unique(_tankStatusesList.begin(), _tankStatusesList.end(),
            [](const auto& status1, const auto& status2)
            {
                return status1.dateTime().secsTo(status2.dateTime()) < 1;
            });

    _tankStatusesList.erase(tankStatusesList_it, _tankStatusesList.end());

    _tankStatusesList.sort();

    return oldSize - _tankStatusesList.size();
}


