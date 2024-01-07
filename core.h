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

namespace LevelGaugeService
{

class Core
    : public QObject
{
    Q_OBJECT

public:
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

private:
    TConfig* _cnf = nullptr;
    Common::TDBLoger* _loger = nullptr;

    QString _errorString;

    std::vector<std::unique_ptr<TankThread>> _tanksThread;  //список конфигураций резервуаров

};

} // namespace LevelGaugeService

#endif // CORE_H
