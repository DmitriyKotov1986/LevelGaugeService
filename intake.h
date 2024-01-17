#ifndef INTAKE_H
#define INTAKE_H

//QT
#include <QObject>

class Intake final
    : public QObject
{
    Q_OBJECT
public:
    explicit Intake(QObject *parent = nullptr);

public slots:
    void start();
    void stop();




private:
    void saveIntake();          //Находит и сохраняет приходы
    TankStatusIterator getStartIntake();    //возвращает итератор на начало приема топлива
    TankStatusIterator getFinishedIntake(); //возвращает итератор на конец приема топлива

};

#endif // INTAKE_H
