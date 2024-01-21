#ifndef CORE_H
#define CORE_H

//STL
#include <memory>
#include <vector>

//QT
#include <QObject>
#include <QList>
#include <QThread>

//My
#include "Common/tdbloger.h"
#include "tconfig.h"
#include "tank.h"
#include "sync.h"

namespace LevelGaugeService
{

class Core
    : public QObject
{
    Q_OBJECT

public:
    Core(const Core&) = delete;
    Core& operator =(const Core&) = delete;
    Core(const Core&&) = delete;
    Core& operator =(const Core&&) = delete;

    explicit Core(QObject *parent = nullptr);
    ~Core();

    void start();
    void stop();

    QString errorString();
    bool isError() const { return !_errorString.isEmpty(); }

signals:
   void stopAll();

private:
    struct TankThread
    {
        std::unique_ptr<Tank> tank;
        std::unique_ptr<QThread> thread;
    };

    struct SyncThread
    {
        std::unique_ptr<Sync> sync;
        std::unique_ptr<QThread> thread;
    };

private:
    bool startSync();

private:
    TConfig* _cnf = nullptr;
    Common::TDBLoger* _loger = nullptr;

    QString _errorString;

    std::vector<std::unique_ptr<TankThread>> _tanksThread;  //список конфигураций резервуаров

    std::unique_ptr<SyncThread> _syncThread; //поток синхронизации с БД АО НИТ

};

} // namespace LevelGaugeService

#endif // CORE_H
