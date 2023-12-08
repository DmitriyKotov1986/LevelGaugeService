#ifndef TLEVELGAUGE_H
#define TLEVELGAUGE_H

#include <QObject>
#include <QSettings>
#include <QSqlDatabase>
#include <QDateTime>
#include <QMap>
#include <QTimer>
#include <QPair>
#include <QQueue>
#include <QRandomGenerator>

class TLevelGauge : public QObject
{
    Q_OBJECT

private:
    enum class TAdditionFlag: quint8
    {
        MEASUMENTS = 1,
        SAVE = 2,
        ADD = 4,
        TEST = 8,
        LIMIT = 16
    }; //флаг способа добавления записи

    struct TStatus
    {
        float Volume = -1;  //текущий объем
        float Mass = -1;    //текущая масса
        float Density = -1; //теккущая плотность
        float Height = -1;  //текущий уровень
        float Temp = -273;  //текущая температура
        TAdditionFlag Flag; //способ добавления записи
    };

private:
    QMap<QDateTime, TStatus> Status;





















    typedef QPair<QDateTime, TStatus> TCurrentStatus;  //текущее состояние резервуара, ключ - дата-время, значение - состояние резервуара

    typedef struct {
        float Diametr = -1; //диаметр
        float Volume = -1; //объем
        QString Product;  //код продукта
        QDateTime LastSaveDateTime = QDateTime::currentDateTime().addYears(-100); //время последнего сохранения
        QDateTime LastIntakeDateTime = QDateTime::currentDateTime().addYears(-100);//время последнего прихода
        TStatus LastWriteStatus;
      //  QMap<float, float> Calibration;
        QMap<QDateTime, QDateTime> ServicePeriod; //периоды сервисного обслуживания (когда не надо отправлять данные на сервер) , ключ -  дата конца, значение - дата начала
    } TTankInfo;

    typedef struct {
        QMap<QDateTime, TStatus> TankStatus; //ключ дата измерения, значение - измерения
        TTankInfo TanksInfo;  //ключ - номер резервуара, значение - его параметры
    } TTank;


















    typedef QMap<QString, TAZS> TAZSS;

    typedef struct {
        float Volume = 1;    //объем
        float Mass = 1;      //масса
        float Density = 0.1; //плотность
        float Height = 1;    //уровеньb
        float Temp = 0.1;    //температура
    } TDelta;

    typedef struct {
        uint16_t Category; //категория сообщения в логе
        QDateTime DateTime;  //дата/время возникновения события
        QString UID; //
        QString Sender;
        QString Msg;
    } TLogMsg;  //запись в очереди логов

    typedef QMap<QDateTime, TLevelGauge::TStatus>::iterator TStatusIterator;
    typedef QMap<QDateTime, TLevelGauge::TStatus>::const_iterator TStatusConstIterator;


private:
    TAZSS AZSS; //ключ - код АЗС, значеие - показания уровнемеров
    qint64 MaxPauseSec = 600;  //максимальный перерыв в поступлении данных, после которго начинается подстановка
    TDelta MaxDelta; //максимально допустимое изменение параметров резервуара за 1 минуту при нормальной работе
    TDelta MaxIntakeDelta;//максимально допустимое изменение параметров резервуара за 1 минуту при приеме топлива
    float DeltaIntakeVolume = 300; //пороговое значение изменения уровня топлива за 10 минут с котого считаем что произошел прием

    QSettings &Config; //конфигурация
    QSqlDatabase DB;   //база данных с исходными данными
    QSqlDatabase LGDB; //база данных в которую пишем результат (НИТ)
    QTimer UpdateTimer; //таймер обновления данных

    quint64 LastLoadID = 0; //ID последней загруженной записи из БД

    QRandomGenerator *rg = QRandomGenerator::global();  //генератор случайных чисел для имитации разброса параметров при измерении в случае подстановки

    QQueue <TLogMsg> LogQueue; //очередь сообщений лога
    QTimer WriteLogTimer; //таймер записи лога

    void SendLogMsg(uint16_t Category, const QString &Msg);  //сохраняет сообщение в лог
    void LoadTanksInfo();  //загружает конфигурацию резервуаров
    void LoadMeasuments();  //загружает текущие измерения
    void LoadLastWriteData();  //загружает ранее сохраненные данные
    void AddStatus(TTank& CurrentTank, const QDateTime& Start, const QDateTime& End, const TStatus& StartAggregate, const TStatus& EndAggregate);  //добавляет недостающие данные, если их нет
    void TestStatus(TTank& CurrentTank, TStatusIterator StartIter); //правильность смещения измеренных величит перед записью
    void TestStatusIter(TStatusIterator Iter, const TDelta& Delta, const TDelta& MaxDelta); //корректирует передаваемые данные
    bool TestServiceIntervel(const TTank& CurrentTank, const QDateTime& DateTime); //возвращает true если заданная емкомсь в указанное время находиться на ремонте
    void SaveIntake(); //обнаруживает и сохраняет приходы топлива
    TStatusConstIterator GetStartIntake(const TTank& CurrentTankconst, TStatusConstIterator StartFindIter); //возвращает итератор на начало приема топлива
    TStatusConstIterator GetFinishedIntake(const TTank& CurrentTank, TStatusConstIterator StartFindIter); //возвращает итератор на конец приема топлива
    void SaveMeasuments(); //сохраняет измерения в БД НИТа
    TStatusConstIterator GetRecordForSave(const QString &AZSCode, quint8 TankNumber);  //возвращает итератор на первую запись с котрой необходимо сохранить данные

public:
    explicit TLevelGauge(QSettings & Config, QObject *parent = nullptr);
    ~TLevelGauge();

    void Start(); //запуск
    void Stop();  //остановка

signals:

private slots:
    void onUpdate();         //сохранение записей
    void onWriteLogTimer();  //сохранение логов
};

#endif // TLEVELGAUGE_H
