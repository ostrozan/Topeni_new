#pragma once



#ifndef DEKLARACE_H
#define DEKLARACE_H


#include <Dhcp.h>
#include <Dns.h>
#include <Ethernet.h>
#include <EthernetClient.h>
#include <EthernetServer.h>
#include <EthernetUdp.h>
#include <SD.h>

#include <DHT.h>
#include <ds3231.h>
#include <Wire.h>
#include <config.h>
#include "eepromdekl.h"

#include <DallasTemperature.h>
#include <OneWire.h>

#define POCET_CIDEL 9
#define ZAP true
#define VYP false
#define NOCNI_PROUD A15


//ethernet
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 2, 47);//169, 254, 28, 100
//IPAddress ip(169, 254, 63, 100);
EthernetServer server(80);






int cidloCO2 = A0;
int CO2value;

typedef enum { manual, automat }EnumControl;
typedef enum {  off,on,disable}EnumSwitch;

/*
tabulka termostatu pro jeden den 
*/
typedef struct
{
	uint16_t cas1;
	uint16_t teplota1;
	uint16_t cas2;
	uint16_t teplota2;
	uint16_t cas3;
	uint16_t teplota3;
	uint16_t cas4;
	uint16_t teplota4;
	uint16_t cas5;
	uint16_t teplota5;
}TimeTempTable;

/*sekce zahrnujici:
-vstup cidla, 
-vystup na SSR 
-promenne pro teploty a vlhkost
-tabulky tydenniho termostatu
-adresu v eeprom
*/
typedef struct
{
	uint16_t ee_adr;
	boolean blokovani;      // zakaz topeni 
	uint16_t currentTemp;   //aktualne nastavena teplota z tabulky termostatu
	int teplotaAkt;    //teplota z cidla
	int16_t vlhkostAkt;     //vlhkost z cidla
	uint16_t teplotaRucne;  //rucni nastaveni teploty
	uint8_t offset;         //hystereze spinani teplotou
	char nazev[10];
	EnumSwitch vystup_prew; //spinany vystup
	EnumSwitch vystup_new;  //spinany vystup
	uint8_t outPin;
    uint8_t inPin;
	TimeTempTable tables[7];//tabulky casu a teplot po-ne
	
	
	

}Sekce;
int16_t hygrooffset[POCET_CIDEL];
bool tmp_blok[POCET_CIDEL];
Sekce sekce[POCET_CIDEL];
Sekce *currSekce;
enum { loznice, pokoj,pracovna,koupelna,venkovni,satna, obyvak,techMistnost,zahrada}EnumSensorPlace;

uint8_t indexDH, indexPT;
uint16_t teplotaNadrz[7];//jedna teplota je pro vzduchotechniku
uint16_t teplotaTmp, casTmp;

struct ts dateTime;
char buff[30];

///for web
#define REQ_BUF_SZ   100
char HTTP_req[REQ_BUF_SZ] = { 0 }; // buffered HTTP request stored as null terminated string
char req_index = 0;              // index into HTTP_req buffer
File webFile;
typedef enum { valueHtml, setHtml }htmlFileEn;
htmlFileEn htmlFile;

typedef struct {
	int a, b, c, d, e,f,g,h;

} IndexStruct;

typedef struct
{
	String a, b, c, d, e,f,g;
}ValueStruct;

uint8_t hdo_temp;
bool hdo_rucne,hdo_rucne_temp;
typedef struct 
{
	uint16_t ee_adr;
    bool enable;
	uint16_t timeStart;
	uint16_t timeStop;
	uint16_t temp;
	EnumSwitch vystup_prew; //spinany vystup
	EnumSwitch vystup_new;  //spinany vystup
	uint8_t outPin;
	
}EnableFve;

EnableFve enableFveObyvak, enableFveZimZahrada, enableFveKoupelna;


typedef struct 
{
	uint16_t ee_adr;
	bool manEnable;
	uint16_t t1Zap;
	uint16_t t1Vyp;
	uint16_t t2Zap;
	uint16_t t2Vyp;
	uint8_t outPin;
	EnumSwitch vystup_prew; //spinany vystup
	EnumSwitch vystup_new;  //spinany vystup

}Spinacky;

Spinacky spin[5];
bool tmp_spin[5];
//ds18b20
// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS A10 //cidla DS18b20
#define TEMPERATURE_PRECISION 9 // Lower resolution

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

int numberOfDevices; // Number of temperature devices found

DeviceAddress tempDeviceAddress; // We'll use this variable to store a found device address
DeviceAddress addr[7];

//cidla

DHT cidloSatna(29, DHT22);//satna//(bzlo 29)
DHT cidloZz(31, DHT22);//zimni zahrada
DHT cidloObyv(33, DHT22);///obyvak//
DHT cidloKoup(35, DHT22);//koupelna//
DHT cidloVenk(37, DHT22);//venkovni 
DHT cidloTech(39, DHT22);//tech. mistnost
DHT cidloPrac(41, DHT22);//pracovna
DHT cidloLoz(43, DHT22);//loznice
DHT cidloPokJ(45, DHT22);//Pokoj J
										 // DHT cidloX(40,DHT22);
//*/
//funkce
void sensorInit();
void porovnejNastavene();
void ctiCo2();
void ctiTeploty();
uint16_t getTemp(uint8_t vstup);
void ctiCas();
void nastavVstupyCidel();
void ulozCas(uint16_t cas, Sekce *sekce, Ee_tab_offset_enum casovy_usek, uint8_t den);
void ulozTeplotu(uint16_t teplota, Sekce *sekce, Ee_tab_offset_enum teplota_pro_usek, uint8_t den);
#endif


