#ifndef LEVELGAUGEHOST_H
#define LEVELGAUGEHOST_H

//Qt
#include <QObject>
#include <QHash>

//My
#include "Common/tdbloger.h"
#include "tconfig.h"

namespace LevelGaugeService
{

class LevelGaugeHost : public QObject
{
    Q_OBJECT

public:
    explicit LevelGaugeHost(QObject *parent = nullptr);
    ~LevelGaugeHost();

    QString errorString();
    bool isError() const { return !_errorString.isEmpty(); }

    void start();
    void stop();

private:
    TConfig* _cnf = nullptr; //настройки
    Common::TDBLoger* _loger = nullptr;

    QSqlDatabase _db;

    QString _errorString;

    QHash<QString, QList<quint8>> _AZSes;

}; //class LevelGaugeHost

} //namespace LevelGaugeService

#endif // LEVELGAUGEHOST_H
