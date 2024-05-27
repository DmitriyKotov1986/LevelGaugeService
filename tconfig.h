#pragma once

//QT
#include <QString>

#include "Common/common.h"

namespace LevelGaugeService
{

class TConfig
{
public:
    static TConfig* config(const QString& configFileName = "");
    static void deleteConfig();

private:
    explicit TConfig(const QString& configFileName);
    ~TConfig();

public:
    bool save();

    //[DATABASE]
    const Common::DBConnectionInfo& dbConnectionInfo() const { return _dbConnectionInfo; };

    //[NIT_DATABASE]
    const Common::DBConnectionInfo& dbNitConnectionInfo() const { return _dbNitConnectionInfo; };

    //[SYSTEM]
    bool sys_DebugMode() const { return _sys_DebugMode; }

    //errors
    QString errorString();
    bool isError() const { return !_errorString.isEmpty(); }

private:
    const QString _configFileName;

    QString _errorString;

    //[DATABASE]
    Common::DBConnectionInfo _dbConnectionInfo;

    //[NIT_DATABASE]
    Common::DBConnectionInfo _dbNitConnectionInfo;

    //[SYSTEM]
    bool _sys_DebugMode = false;

};

} //namespace RegService
