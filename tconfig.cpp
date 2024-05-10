#include "tconfig.h"

#include <QSettings>
#include <QFileInfo>
#include <QDebug>
#include <QFileInfo>

using namespace  LevelGaugeService;
using namespace Common;

//static
static TConfig* configPtr = nullptr;

TConfig* TConfig::config(const QString& configFileName)
{
    if (configPtr == nullptr)
    {
        Q_ASSERT(!configFileName.isEmpty());
        configPtr = new TConfig(configFileName);
    }

    return configPtr;
};

void TConfig::deleteConfig()
{
    Q_CHECK_PTR(configPtr);

    if (configPtr != nullptr)
    {
        delete configPtr;
        configPtr = nullptr;
    }
}

//public
TConfig::TConfig(const QString& configFileName) :
    _configFileName(configFileName)
{
    if (_configFileName.isEmpty()) {
        _errorString = "Configuration file name cannot be empty";

        return;
    }
    if (!QFileInfo::exists(_configFileName)) {
        _errorString = "Configuration file not exist. File name: " + _configFileName;

        return;
    }

    qDebug() << QString("%1 %2").arg(QTime::currentTime().toString(SIMPLY_TIME_FORMAT)).arg("Reading configuration from " +  _configFileName);

    QSettings ini(_configFileName, QSettings::IniFormat);

    QStringList groups = ini.childGroups();
    if (!groups.contains("DATABASE"))
    {
        _errorString = "Configuration file not contains [DATABASE] group";

        return;
    }
    if (!groups.contains("NIT_DATABASE"))
    {
        _errorString = "Configuration file not contains [NIT_DATABASE] group";

        return;
    }

    //Database
    ini.beginGroup("DATABASE");
    _dbConnectionInfo.db_Driver = ini.value("Driver", "QODBC").toString();
    _dbConnectionInfo.db_DBName = ini.value("DataBase", "DB").toString();
    _dbConnectionInfo.db_UserName = ini.value("UID", "").toString();
    _dbConnectionInfo.db_Password = ini.value("PWD", "").toString();
    _dbConnectionInfo.db_ConnectOptions = ini.value("ConnectionOptions", "").toString();
    _dbConnectionInfo.db_Port = ini.value("Port", "").toUInt();
    _dbConnectionInfo.db_Host = ini.value("Host", "localhost").toString();
    ini.endGroup();

    //AO Nit Database
    ini.beginGroup("NIT_DATABASE");
    _dbNitConnectionInfo.db_Driver = ini.value("Driver", "QODBC").toString();
    _dbNitConnectionInfo.db_DBName = ini.value("DataBase", "DB").toString();
    _dbNitConnectionInfo.db_UserName = ini.value("UID", "").toString();
    _dbNitConnectionInfo.db_Password = ini.value("PWD", "").toString();
    _dbNitConnectionInfo.db_ConnectOptions = ini.value("ConnectionOptions", "").toString();
    _dbNitConnectionInfo.db_Port = ini.value("Port", "").toUInt();
    _dbNitConnectionInfo.db_Host = ini.value("Host", "localhost").toString();
    ini.endGroup();

    //System
    ini.beginGroup("SYSTEM");

    _sys_DebugMode = ini.value("DebugMode", "0").toBool();

    ini.endGroup();
}

TConfig::~TConfig()
{
   save();
}

bool TConfig::save()
{
    QSettings ini(_configFileName, QSettings::IniFormat);

    if (!ini.isWritable()) {
        _errorString = "Can not write configuration file " +  _configFileName;

        return false;
    }

    ini.clear();

    //Database
    ini.beginGroup("DATABASE");

    ini.remove("");

    ini.setValue("Driver", _dbConnectionInfo.db_Driver);
    ini.setValue("DataBase", _dbConnectionInfo.db_DBName);
    ini.setValue("UID", _dbConnectionInfo.db_UserName);
    ini.setValue("PWD", _dbConnectionInfo.db_Password);
    ini.setValue("ConnectionOprions", _dbConnectionInfo.db_ConnectOptions);
    ini.setValue("Port", _dbConnectionInfo.db_Port);
    ini.setValue("Host", _dbConnectionInfo.db_Host);

    ini.endGroup();

    //Database
    ini.beginGroup("NIT_DATABASE");

    ini.remove("");

    ini.setValue("Driver", _dbNitConnectionInfo.db_Driver);
    ini.setValue("DataBase", _dbNitConnectionInfo.db_DBName);
    ini.setValue("UID", _dbNitConnectionInfo.db_UserName);
    ini.setValue("PWD", _dbNitConnectionInfo.db_Password);
    ini.setValue("ConnectionOprions", _dbNitConnectionInfo.db_ConnectOptions);
    ini.setValue("Port", _dbNitConnectionInfo.db_Port);
    ini.setValue("Host", _dbNitConnectionInfo.db_Host);

    ini.endGroup();

    //System
    ini.beginGroup("SYSTEM");

    ini.remove("");

    ini.setValue("DebugMode", _sys_DebugMode);

    ini.endGroup();

    //сбрасываем буфер
    ini.sync();

    if (_sys_DebugMode)
    {
        qDebug() << QString("%1 %2").arg(QTime::currentTime().toString(SIMPLY_TIME_FORMAT)).arg("Save configuration to " +  _configFileName);
    }

    return true;
}

QString TConfig::errorString()
{
    auto res = _errorString;
    _errorString.clear();

    return res;
}

