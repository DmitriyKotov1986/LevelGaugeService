#ifndef CORE_H
#define CORE_H

#include <QObject>
#include <QList>

//My
#include "Common/tdbloger.h"
#include "tank.h"
#include "tconfig.h"

namespace LevelGaugeService
{

class Core : public QObject
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
   void loadTankConfig();

private:
    TConfig* _cnf = nullptr;
    Common::TDBLoger* _loger = nullptr;

    QSqlDatabase _db;

    QString _errorString;

    QList<Tank::TankConfig> _tanks;  //список конфигураций резервуаров

};

} // namespace LevelGaugeService

#endif // CORE_H
