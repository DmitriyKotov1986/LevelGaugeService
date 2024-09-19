//STL
#include <exception>

//QT
#include <QSqlQuery>
#include <QMutex>
#include <QMutexLocker>

//My
#include "Common/common.h"

#include "tankconfig.h"

using namespace LevelGaugeService;
using namespace Common;

static const QString TANKS_CONFIG_CONNECTION_TO_DB_NAME = "TANKS_CONFIG_DB";
static QMutex lastTimeMutex;

//statis
TankConfig::Status TankConfig::intToStatus(quint8 status)
{
    switch (status)
    {
    case static_cast<quint8>(Status::REPAIR): return Status::REPAIR;
    case static_cast<quint8>(Status::INTAKE): return Status::INTAKE;
    case static_cast<quint8>(Status::PUMPING_OUT): return Status::PUMPING_OUT;
    case static_cast<quint8>(Status::STUGLE): return Status::STUGLE;
    case static_cast<quint8>(Status::UNDEFINE): return Status::UNDEFINE;
    default: Q_ASSERT(false);
    }

    return Status::UNDEFINE;
}

TankConfig::Mode TankConfig::intToMode(quint8 mode)
{
    switch (mode)
    {
    case static_cast<quint8>(Mode::AZS): return Mode::AZS;
    case static_cast<quint8>(Mode::OIL_DEPOT): return Mode::OIL_DEPOT;
    case static_cast<quint8>(Mode::UNDEFINE): return Mode::UNDEFINE;
    default: Q_ASSERT(false);
    }

    return Mode::UNDEFINE;
}

TankConfig::Type TankConfig::intToType(quint8 type)
{
    switch (type)
    {
    case static_cast<quint8>(Type::HORIZONTAL): return Type::HORIZONTAL;
    case static_cast<quint8>(Type::VERTICAL): return Type::VERTICAL;
    case static_cast<quint8>(Type::UNDEFINE): return Type::UNDEFINE;
    default: Q_ASSERT(false);
    }

    return Type::UNDEFINE;
}

TankConfig::ProductStatus TankConfig::intToProductStatus(quint8 productStatus)
{
    switch ( productStatus)
    {
    case static_cast<quint8>(ProductStatus::PASPORT): return ProductStatus::PASPORT;
    case static_cast<quint8>(ProductStatus::UNPASPORT): return ProductStatus::UNPASPORT;
    case static_cast<quint8>(ProductStatus::UNDEFINE): return ProductStatus::UNDEFINE;
    default: Q_ASSERT(false);
    }

    return ProductStatus::UNDEFINE;
}

//class
TankConfig::TankConfig(const TankID& id,
                       const qint64 remoteApplicantId, const qint64 remoteObjectId, const qint64 remoteTankId, const QString& name, const QString& remoteBearerToken, const QUrl& remoteBaseUrl,
                       float totalVolume, float diametr, qint64 timeShift, Mode mode, Type type,
                       const Delta& deltaMax, const Delta& deltaIntake, float deltaIntakeHeight, float deltaPumpingOutHeight, Status status,
                       const QString& product, ProductStatus productStatus,
                       QObject* parent /* = nullptr */)
    : QObject{parent}
    , _id(id)
    , _remoteApplicantId(remoteApplicantId)
    , _remoteObjectId(remoteObjectId)
    , _remoteTankId(remoteTankId)
    , _name(name)
    , _remoteBearerToken(remoteBearerToken)
    , _remoteBaseUrl(remoteBaseUrl)
    , _totalVolume(totalVolume)
    , _diametr(diametr)
    , _timeShift(timeShift)
    , _deltaMax(deltaMax)
    , _deltaIntake(deltaIntake)
    , _deltaIntakeHeight(deltaIntakeHeight)
    , _deltaPumpingOutHeight(deltaPumpingOutHeight)
    , _status(status)
    , _mode(mode)
    , _type(type)
    , _product(product)
    , _productStatus(productStatus)
{
    Q_ASSERT(_id.tankNumber() != 0);
    Q_ASSERT(_remoteApplicantId != 0);
    Q_ASSERT(_remoteObjectId != 0);
    Q_ASSERT(_remoteTankId != 0);
    Q_ASSERT(!_id.levelGaugeCode().isEmpty());
    Q_ASSERT(!_name.isEmpty());
    Q_ASSERT(!_remoteBearerToken.isEmpty());
    Q_ASSERT(!_remoteBaseUrl.isEmpty() && _remoteBaseUrl.isValid());
    Q_ASSERT(_totalVolume > 0.0f);
    Q_ASSERT(_diametr > 0.0f);
    Q_ASSERT(_deltaMax.check());
    Q_ASSERT(_deltaIntake.check());
    Q_ASSERT(_deltaIntakeHeight > 0.0f);
    Q_ASSERT(_deltaPumpingOutHeight > 0.0f);
    Q_ASSERT(!_product.isEmpty());
    Q_ASSERT(_type != Type::UNDEFINE);
    Q_ASSERT(_mode != Mode::UNDEFINE);
    Q_ASSERT(_productStatus != ProductStatus::UNDEFINE);

    makeLimits();
}


void TankConfig::makeLimits()
{
    _limits.density = std::make_pair<float>(350.0f, 1200.0f);
    _limits.height  = std::make_pair<float>(0.0f, _diametr);
    _limits.mass    = std::make_pair<float>(0.0f, _totalVolume * _limits.density.second);
    _limits.volume  = std::make_pair<float>(0.0f, _totalVolume);
    _limits.temp    = std::make_pair<float>(-50.0f, 100.0f);
}

const TankID &TankConfig::tankId() const
{
    return _id;
}

qint64 TankConfig::remoteTankId() const
{
    return _remoteTankId;
}

qint64 TankConfig::remoteObjectId() const
{
    return _remoteObjectId;
}

qint64 TankConfig::remoteApplicantId() const
{
    return _remoteApplicantId;
}

const QString& TankConfig::name() const
{
    return _name;
}

const QString &TankConfig::remoteBearerToken() const
{
    return _remoteBearerToken;
}

const QUrl &TankConfig::remoteBaseUrl() const
{
    return _remoteBaseUrl;
}

float TankConfig::totalVolume() const
{
    return _totalVolume;
}

float TankConfig::diametr() const
{
    return _diametr;
}

qint64 TankConfig::timeShift() const
{
    return _timeShift;
}

const TankConfig::Limits &TankConfig::limits() const
{
    return _limits;
}

const TankConfig::Delta &TankConfig::deltaMax() const
{
    return _deltaMax;
}

const TankConfig::Delta &TankConfig::deltaIntake() const
{
    return _deltaIntake;
}

float TankConfig::deltaIntakeHeight() const
{
    return _deltaIntakeHeight;
}

float TankConfig::deltaPumpingOutHeight() const
{
    return _deltaPumpingOutHeight;
}

TankConfig::Status TankConfig::status() const
{
    return _status;
}

TankConfig::Mode TankConfig::mode() const
{
    return _mode;
}

TankConfig::Type TankConfig::type() const
{
    return _type;
}

const QString &TankConfig::product() const
{
    return _product;
}

TankConfig::ProductStatus TankConfig::productStatus() const
{
    return _productStatus;
}

const QDateTime &TankConfig::lastMeasuments() const
{
    QMutexLocker<QMutex> locker(&lastTimeMutex);

    return _lastMeasuments;
}

void TankConfig::setLastMeasuments(const QDateTime &lastTime)
{
    Q_ASSERT(_lastMeasuments <= lastTime);

    QMutexLocker<QMutex> locker(&lastTimeMutex);

    _lastMeasuments = lastTime;

    emit lastMeasuments(_id, lastTime);
}

const QDateTime &TankConfig::lastSave() const
{
    QMutexLocker<QMutex> locker(&lastTimeMutex);

    return _lastSave;
}

void TankConfig::setLastSave(const QDateTime &lastTime)
{
    Q_ASSERT(_lastSave <= lastTime);

    QMutexLocker<QMutex> locker(&lastTimeMutex);

    _lastSave = lastTime;

    emit lastSave(_id, lastTime);
}

const QDateTime &TankConfig::lastSend() const
{
    QMutexLocker<QMutex> locker(&lastTimeMutex);

    return _lastSend;
}

void TankConfig::setLastSend(const QDateTime &lastTime)
{
    Q_ASSERT(_lastSend <= lastTime);

    QMutexLocker<QMutex> locker(&lastTimeMutex);

    _lastSend = lastTime;

    emit lastSend(_id, lastTime);
}

const QDateTime &TankConfig::lastIntake() const
{
    QMutexLocker<QMutex> locker(&lastTimeMutex);

    return _lastIntake;
}

void TankConfig::setLastIntake(const QDateTime &lastTime)
{
    Q_ASSERT(_lastIntake <= lastTime);

    QMutexLocker<QMutex> locker(&lastTimeMutex);

    _lastIntake = lastTime;

    emit lastIntake(_id, lastTime);
}

const QDateTime &TankConfig::lastSendIntake() const
{
    QMutexLocker<QMutex> locker(&lastTimeMutex);

    return _lastSendIntake;
}

void TankConfig::setLastSendIntake(const QDateTime &lastTime)
{
    Q_ASSERT(_lastSendIntake <= lastTime);

    QMutexLocker<QMutex> locker(&lastTimeMutex);

    _lastSendIntake = lastTime;

    emit lastSendIntake(_id, lastTime);
}
