#ifndef ATM90E26_H
#define ATM90E26_H
#include <Arduino.h>
#include <SPI.h>
//Struct to hold the energy meter data
struct MeterData {
    float voltage;
    float current;
    float activePower;
    float powerFactor;
    float energyKWh;
    float frequency;
    bool valid;

};



class ATM90E26 {
    public:
    bool begin();
    bool readMeasurement();
    MeterData getMeterData() const;

    private:
    MeterData data;
    uint16_t readRegister(uint8_t address);
    void writeRegister(uint8_t address, uint16_t value);
};

/*float readVoltage();
float readCurrent();
float readActivePower();
float readPowerFactor();
float readEnergyKWh();*/






#endif // ATM90E26_H