#ifndef TANKID_H
#define TANKID_H

//STL
#include "unordered_map"

//QT
#include <QMetaType>
#include <QHash>
#include <QString>

namespace LevelGaugeService
{

class TankID
{
public:
    TankID() = default;

    explicit TankID(const QString& levelGaugeCode, quint8 tankNumber);

    const QString& levelGaugeCode() const;
    quint8 tankNumber() const;

    bool operator==(const TankID& other) const;

private:
    QString _levelGaugeCode; //код измерительной системы
    quint8 _tankNumber = 0; //номер резервуара

}; //class TankID

using TankIDList = QList<TankID>;

//Hash for QT
inline size_t qHash(const TankID &key, size_t seed) noexcept
{
    return qHashMulti(seed, key.levelGaugeCode(), key.tankNumber());
}

} //namespace LevelGaugeService

//Hash for STL
template<>
struct std::hash<LevelGaugeService::TankID>
{
    inline std::size_t operator()(const LevelGaugeService::TankID& id) const noexcept
    {
        return qHash(id, 0);
    }
};

Q_DECLARE_METATYPE(LevelGaugeService::TankID)

#endif // TANKID_H
