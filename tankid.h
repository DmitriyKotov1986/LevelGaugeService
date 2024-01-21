#ifndef TANKID_H
#define TANKID_H

//QT
#include <QHash>
#include <QString>

namespace LevelGaugeService
{

class TankID
{
public:
    TankID() = delete;

    explicit TankID(const QString& levelGaugeCode, quint8 tankNumber);

    const QString& levelGaugeCode() const;
    quint8 tankNumber() const;

    bool operator==(const TankID& other) const;

private:
    QString _levelGaugeCode;
    quint8 _tankNumber = 0;

}; //class TankID

inline size_t qHash(const TankID &key, size_t seed) noexcept
{
    return qHashMulti(seed, key.levelGaugeCode(), key.tankNumber());
}

} //namespace LevelGaugeService

#endif // TANKID_H
