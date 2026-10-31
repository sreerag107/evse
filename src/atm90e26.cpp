#include "atm90e26.h"



static const uint8_t atm90e26_cs_pin = 4;// Chip select pin for ATM90E26


static const uint8_t SoftReset = 0x00; // Soft reset register address
static const uint8_t SysStatus = 0x01; // System status register address
static const uint8_t FuncEn= 0x02;// Function enable register address
static const uint8_t SagTh = 0x03; // Sag threshold register address
static const uint8_t LastData = 0x06; // Last data register address
static const uint8_t Ugain = 0x31; // Voltage gain register address
static const uint8_t IgainL = 0x32; // Current gain register address
static const uint8_t Lgain = 0x23; // Line calibration gain register address
static const uint8_t Lphi = 0x24; // Line calibration angle register address
static const uint8_t MMode = 0x2B; // Metering mode register address
static const uint8_t PlconstH = 0x21; // Active power constant high register address
static const uint8_t PlconstL = 0x22; // Active power constant low
static const uint8_t Ngain = 0x25; // Neutral gain register address
static const uint8_t Nphi = 0x26; // Neutral angle register address
static const uint8_t PstartTh = 0x27; // Active power start threshold register address
static const uint8_t PNolTh = 0x28; // Active power no-load threshold register address
static const uint8_t QstartTh = 0x29; // Reactive power start threshold register address
static const uint8_t QNolTh = 0x2A; // Reactive power
static const uint8_t CS1=0x2C; //checksum register address
    
static const uint8_t IgainN=0x33; // Neutral current gain register address
static const uint8_t Uoffset=0x34; // Voltage offset register address
static const uint8_t IoffsetL=0x35; // Current offset register address
static const uint8_t IoffsetN=0x36; // Neutral current offset register address
static const uint8_t PoffsetL=0x37; // Active power offset register address
static const uint8_t QoffsetL=0x38; // Reactive power offset register address
static const uint8_t PoffsetN=0x39; // Active power offset register address
static const uint8_t QoffsetN=0x3A; // Reactive power offset register address
static const uint8_t CS2=0x3B; //checksum register address
//Caliberation registers
static const uint8_t CalStart = 0x20; // Calibration start register address
static const uint8_t AdjStart = 0x30; // Calibration end register address


//Messurement registers
static const uint8_t Irms = 0x48; // Current measurement register address
static const uint8_t Urms = 0x49; // Voltage measurement register address
static const uint8_t Pmean = 0x4A; // Active power measurement register address
static const uint8_t Freq = 0x4C; // Frequency measurement register address
static const uint8_t Pfactor = 0x4D; // Power factor measurement register address
static const uint8_t APenergy = 0x40; // Active energy measurement register address


//Values
const uint16_t mmode = 0x9422;
const uint32_t plConst = 1429876UL;
const uint16_t lGain = 7481;

const uint16_t uGain = 26400;  // Starting value
const uint16_t iGainL = 31251; // Starting value
const uint16_t iGainN = 0x7530;
//Spi Configuration
static SPISettings atm90e26spiSettings(200000, MSBFIRST, SPI_MODE3); // 200 kHz, MSB first, mode 3

bool ATM90E26::begin(){
    pinMode(atm90e26_cs_pin, OUTPUT);
    digitalWrite(atm90e26_cs_pin, HIGH); // Set CS pin high to deselect the device
    //SPI.begin(18,19,23,atm90e26_cs_pin); // Initialize SPI
    data = {};
    data.valid = false;

    writeRegister(SoftReset, 0x789A); // Soft reset the device
    delay(10); // Wait for the device to reset

    writeRegister(FuncEn, 0x0030); // Enable measurement functions
    if(readRegister(LastData) != 0x0030){
        return false; // Check if the device is responding correctly
    }
    writeRegister(SagTh,0x1F2F); // Set sag threshold

    // Calibration process from register 0x21 to 0x2b

    writeRegister(CalStart,0x5678); // Start calibration

    writeRegister(MMode, mmode); // Set metering mode
    writeRegister(PlconstH, (uint16_t)(plConst >> 16)); // Set active power constant high
    writeRegister(PlconstL, (uint16_t)(plConst & 0xFFFF)); // Set active power constant low
    writeRegister(Lgain, lGain); // Set line calibration gain
    writeRegister(Lphi, 0x0000); // Set line calibration angle
    writeRegister(Ngain, 0x0000); // Set neutral gain
    writeRegister(Nphi, 0x0000); // Set neutral angle
    writeRegister(PstartTh, 0x08BD); // Set active power start threshold 
    writeRegister(PNolTh, 0x0000); // Set active power no-load threshold
    writeRegister(QstartTh, 0x0AEC); // Set reactive power start threshold
    writeRegister(QNolTh, 0x0000); // Set reactive power no-load threshold
    
    
    
    
    // Checksum calculation

    uint8_t sum1 = 0;
    uint8_t xor1 = 0;

    uint8_t regvalues[]={
        (uint8_t)(mmode >> 8), (uint8_t)(mmode & 0xFF),
        (uint8_t)(plConst >> 24), (uint8_t)((plConst >> 16) & 0xFF), (uint8_t)((plConst >> 8) & 0xFF), (uint8_t)(plConst & 0xFF),
        (uint8_t)(lGain >> 8), (uint8_t)(lGain & 0xFF),
        0x08,0xBD,
        0x0A,0xEC
    };

    for (uint8_t i = 0; i < sizeof(regvalues); i++) {
        sum1 += regvalues[i];
        xor1 ^= regvalues[i];
    }

    uint16_t checksum = (uint16_t)(xor1 << 8) | sum1;
    writeRegister(CS1, checksum); // Write checksum to the device
    delay(10); // Wait for the device to process the checksum


//calibration process from register 0x31 to 0x3A
writeRegister(AdjStart,0x5678); // Start calibration

writeRegister(Ugain, uGain); // Set voltage gain
writeRegister(IgainL, iGainL); // Set current gain for line
writeRegister(IgainN, iGainN); // Set current gain for neutral
writeRegister(Uoffset, 0x0000); // Set voltage offset
writeRegister(IoffsetL, 0x0000); // Set current offset for line
writeRegister(IoffsetN, 0x0000); // Set current offset for neutral
writeRegister(PoffsetL, 0x0000); // Set active power offset for line
writeRegister(QoffsetL, 0x0000); // Set reactive power offset for line
writeRegister(PoffsetN, 0x0000); // Set active power offset for neutral 
writeRegister(QoffsetN, 0x0000); // Set reactive power offset for neutral


uint8_t regvalues2[]={
    (uint8_t)(uGain >> 8), (uint8_t)(uGain & 0xFF),
    (uint8_t)(iGainL >> 8), (uint8_t)(iGainL & 0xFF),
    (uint8_t)(iGainN >> 8), (uint8_t)(iGainN & 0xFF)
};

uint8_t sum2 = 0;
uint8_t xor2 = 0;

for (uint8_t i = 0; i < sizeof(regvalues2); i++) {
    sum2 += regvalues2[i];
    xor2 ^= regvalues2[i];
}
uint16_t checksum2 = (uint16_t)(xor2 << 8) | sum2;
writeRegister(CS2, checksum2); // Write checksum to the device
delay(10); // Wait for the device to process the checksum


writeRegister(CalStart, 0x8765);
writeRegister(AdjStart, 0x8765);

uint16_t status = readRegister(SysStatus); 
if ((status & 0xC000)){

    Serial.println("ATM90E26 initialization failed: Checksum cs 1 error.");
    return false;
};
if ((status & 0x3000)){
    Serial.println("ATM90E26 initialization failed: Checksum cs 2 error.");
    return false;
    
} 

    return true;
}

bool ATM90E26::readMeasurement(){

    if(!checkCommunication()){
        Serial.println("ATM90E26 communication error.");
        data.valid = false;
        return false;
    }
    data.voltage = (float)readRegister(Urms) * 0.01; // Convert to volts
    data.current = (float)readRegister(Irms) * 0.001; // Convert to amperes
    int16_t rawActivePower = (int16_t)readRegister(Pmean);
    data.activePower = (float)rawActivePower; // Convert to watts
    data.frequency = (float)readRegister(Freq) * 0.01; // Convert to hertz

    uint16_t rawPowerFactor = (uint16_t)readRegister(Pfactor);

    if(rawPowerFactor & 0x8000){
        data.powerFactor = -((float)(rawPowerFactor & 0x7FFF) * 0.001); // Convert to negative power factor
    }
    else{
        data.powerFactor = (float)rawPowerFactor * 0.001; // Convert to positive power factor
    }

    uint16_t rawEnergy = readRegister(APenergy);
    const float pulseConstant = 3200.0; // Example pulse constant, adjust as needed
    float newEnergyKWh= (float)(rawEnergy / (pulseConstant*10)); // Convert to kWh
    data.energyKWh += newEnergyKWh;

    return true;



}



MeterData ATM90E26::getMeterData() const {
    return data;
}   






uint16_t ATM90E26::readRegister(uint8_t address){


    SPI.beginTransaction(atm90e26spiSettings);
    digitalWrite(atm90e26_cs_pin, LOW); // Select the device
    SPI.transfer(address | 0x80); // Send read command with address
    delayMicroseconds(4);
    uint8_t msb = SPI.transfer(0x00); // Read high byte
    uint8_t lsb = SPI.transfer(0x00); // Read low byte
    digitalWrite(atm90e26_cs_pin, HIGH); // Deselect the device
    SPI.endTransaction();
    uint16_t value = (msb << 8) | lsb; // Combine bytes into 16-bit value
   
    return value;
}

bool ATM90E26::checkCommunication(){
    uint16_t value = readRegister(FuncEn);
    if(value == 0x0030){
        return true; // Communication is successful
    } else {
        return false; // Communication failed
    }

}   

void ATM90E26::writeRegister(uint8_t address, uint16_t value){
    SPI.beginTransaction(atm90e26spiSettings);
    digitalWrite(atm90e26_cs_pin, LOW); // Select the device
    SPI.transfer(address & 0x7F); // Send write command with address
    delayMicroseconds(4);
    SPI.transfer((uint8_t)(value >> 8)); // Send high byte
    SPI.transfer((uint8_t)(value & 0xFF)); // Send low byte
    digitalWrite(atm90e26_cs_pin, HIGH); // Deselect the device
    SPI.endTransaction();
}

