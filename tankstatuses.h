#pragma once

//STL
#include <memory>
#include <map>
#include <list>

//QT
#include <QDateTime>

//My
#include "tankstatus.h"

namespace LevelGaugeService
{

using TankStatuses = std::map<QDateTime, std::unique_ptr<TankStatus>>;

class TankStatusesList
{
public:
    using TankStatusListConteiner = std::list<TankStatus>;
    using TankStatusListIterator = TankStatusListConteiner::iterator;
    using TankStatusListIteratorConst = TankStatusListConteiner::const_iterator;

public:
    explicit TankStatusesList(qsizetype size = 0);

    TankStatusListIterator begin();
    TankStatusListIterator end();

    TankStatusListIteratorConst begin() const;
    TankStatusListIteratorConst end() const;

    bool empty() const;
    qsizetype size() const;

    void emplace_back(TankStatus&& status);
    void push_back(const TankStatus& status);

    qsizetype filtered(const QDateTime& startDateTime);

private:
    TankStatusListConteiner _tankStatusesList;

};

/*
auto operator+(LevelGaugeService::TankStatusesList::TankStatusListIterator iterator, int next)
{
    return std::next(iterator, next);
}

auto operator-(LevelGaugeService::TankStatusesList::TankStatusListIterator iterator, int next)
{
    return std::next(iterator, -next);
}

auto operator-(LevelGaugeService::TankStatusesList::TankStatusListIterator iteratorFirst, LevelGaugeService::TankStatusesList::TankStatusListIterator iteratorSecond)
{
    return std::distance(iteratorFirst, iteratorSecond);
}

auto operator<(LevelGaugeService::TankStatusesList::TankStatusListIterator iteratorFirst, LevelGaugeService::TankStatusesList::TankStatusListIterator iteratorSecond)
{
    return std::distance(iteratorFirst, iteratorSecond) > 0;
}
*/

} //namespace LevelGaugeService

Q_DECLARE_METATYPE(LevelGaugeService::TankStatusesList);

