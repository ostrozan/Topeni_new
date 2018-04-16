/*
zmena 24.11.16 /prohozeni vstupy - vystupy
* vstupy cidel dht 22 -  spinany vystup
*                   29 -> 22  
*                   31 -> 24  
*                   33 -> 26 
*                   35 -> 28
*                   37 -> nema vystup
*                   39 -> 30
*                   41 -> 32 
*                   43 -> 34
*                   45 -> 36
*                 
*
* vstupy cidel PT100
*                    A0
*                    A1
*                    A2
*
* vstup cidla CO2
*                    A5
*/


#include <DHT_U.h>
#include <EEPROM.h>
#include <Wire.h>
#include <ds3231.h>
#include <config.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <DHT.h>
#include <SD.h>
#include <EthernetUdp.h>
#include <EthernetServer.h>
#include <EthernetClient.h>
#include <Ethernet.h>
#include <Dns.h>
#include <Dhcp.h>
#include "deklarace.h"

void setup()
{
	//offset cidel mereni vlhkosti
	hygrooffset[0] =  - 122;
	hygrooffset[1] = -110;
	hygrooffset[2] = 176;
	hygrooffset[3] = -99;
	hygrooffset[4] = 0;//-63;venkovni bez ofsetu
	hygrooffset[5] = 20;
	hygrooffset[6] = 318;
	hygrooffset[7] = -103;
	hygrooffset[8] = -72;
	

    
	Serial.begin(9600);       // for debugging
    SettingIO();
							  // initialize SD card
	Serial.println("Initializing SD card...");
	if (!SD.begin(4)) {
		Serial.println("ERROR - SD card initialization failed!");
		return;    // init failed
	}
	Serial.println("SUCCESS - SD card initialized.");
	// check for settings.htm file
	if (!SD.exists("topeni.htm")) {
		Serial.println("ERROR - Can't find topeni.html file!");
		return;  // can't find index file
	}
	Serial.println("SUCCESS - Found .html file.");


	Ethernet.begin(mac, ip);  // initialize Ethernet device
	server.begin();           // start to listen for clients

							  //  Wire.begin();
	DallasInit();
	sensorInit();
	Wire.begin();
	DS3231_init(DS3231_INTCN);
	ee_adrInit();
    //defaultValues();
	restoreData();
	test();
}

void loop()
{
	ctiTeploty();
	ctiTeplotyNadrz();
	CtiHdoSignal();
	ctiCo2();
	ctiCas();
	webServer();
}

///////////////////////////////////////////////////////////////
//RIZENI
///////////////////////////////////////////////////////////////

#pragma region TEPLOMERY

/*
cte vstup pro kontakt rele HDO
*/
void CtiHdoSignal()
{

	uint8_t pom=digitalRead(NOCNI_PROUD);
	bool pom_rucne = hdo_rucne;
   
	if (pom != hdo_temp || pom_rucne != hdo_rucne_temp)
	{
		//if (pom_rucne != hdo_rucne_temp)Serial.println("xxxxxxxxxxx");

		hdo_temp= pom ;
		hdo_rucne_temp = pom_rucne;
	
		if (hdo_temp == 0 || hdo_rucne == true )
		{
			for (int i = 0;i < POCET_CIDEL;i++)
			{
				if (i == 4)continue;//sekce 4 venkovni cidlo nema vystup

				sekce[i].blokovani = tmp_blok[i];
			
				digitalWrite(sekce[i].outPin, sekce[i].vystup_new);
			}
		}
			
		else if (hdo_temp == 1 || hdo_rucne == false)
		{
			for (int i = 0;i < POCET_CIDEL;i++)
			{
				if (i == 4)continue;//sekce 4 venkovni cidlo nema vystup
                //sekce[i].vystup_new = off;
				digitalWrite(sekce[i].outPin, LOW);
				
				tmp_blok[i] = sekce[i].blokovani;
				sekce[i].blokovani = true;
			}
		}
		
	}


    if (hdo_temp == 0 || hdo_rucne == true)porovnejNastavene();
}


/*
cte analogovy vstup cidla CO2
*/
void ctiCo2()
{
	int val = analogRead(cidloCO2);
	CO2value = val;
	/* 
	Serial.print("CO2");
	 Serial.println(CO2value);
//*/
}

/*cte cidla DS18b20 na nadrzi*/
void ctiTeplotyNadrz()
{
	float tmp;
	sensors.requestTemperatures(); // Send the command to get temperatures
	for (int i = 0;i<7; i++)//numberOfDevices
	{

		// Search the wire for address
		if (sensors.getAddress(tempDeviceAddress, i))
		{
			//Serial.print("OK");Serial.println(i);
			tmp = sensors.getTempC(addr[i]);
            teplotaNadrz[i] = (uint16_t)(tmp * 10);
			//Serial.println(teplotaNadrz[i]);
		}//*/
	}
	
}

/*
pri kazdem prubehu cte hodnotu teploty a vlhkosti z jednoho cidla (poradi podle indexu)
*/
void ctiTeploty()
{
	indexDH++;
	if (indexDH>POCET_CIDEL - 1)indexDH = 0; 
	//if (indexDH == 0)
	//{
		sekce[indexDH].teplotaAkt =  getTemp(indexDH);// (//uint16_t)pom;
		sekce[indexDH].vlhkostAkt =  getHumidity(indexDH);
	//}
	//else
	//{
	//sekce[indexDH].teplotaAkt = 200;// getTemp(indexDH);// (//uint16_t)pom;
	//sekce[indexDH].vlhkostAkt = 500;// getHumidity(indexDH);
	//}

	if(sekce[indexDH].vlhkostAkt)sekce[indexDH].vlhkostAkt +=hygrooffset[indexDH];
	
    //test
	/*/ Serial.print(sekce[indexDH].nazev);
	char temp[40];
	sprintf(temp,"%u t = %u,%u   h = %u,%u  %u \n",indexDH+1,
	sekce[indexDH].teplotaAkt/10,sekce[indexDH].teplotaAkt%10,
	sekce[indexDH].vlhkostAkt/10,sekce[indexDH].vlhkostAkt%10,hygrooffset[indexDH]);
	Serial.print(temp);
	//end test*/
}


/*
ziska teplotu z cidla DHT22
*/
uint16_t getTemp(uint8_t vstup)
{
	uint16_t retval;

	switch (vstup)
	{
	case 0: retval = (uint16_t)cidloLoz.readTemperature() * 10;break;
	case 1: retval = (uint16_t)cidloPokJ.readTemperature() * 10;break;
	case 2: retval = (uint16_t)cidloPrac.readTemperature() * 10;break;
	case 3: retval = (uint16_t)cidloKoup.readTemperature() * 10;break;
	case 4: retval = (uint16_t)cidloVenk.readTemperature() * 10;break;
	case 5: retval = (uint16_t)cidloSatna.readTemperature() * 10;break;
	case 6: retval = (uint16_t)cidloObyv.readTemperature() * 10;break;
	case 7: retval = (uint16_t)cidloTech.readTemperature() * 10;break;
	case 8: retval = (uint16_t)cidloZz.readTemperature() * 10;break;
		//case 9: retval = cidlo1.readTemperature()*10; break;   
	}

	//Serial.println(retval);
	//return 200 + vstup;//
	return retval;
}

/*
ziska vlhkost z cidla DHT22

*/
uint16_t getHumidity(uint8_t vstup)
{
	//   Serial.println("ms");
	// long a=micros();
	uint16_t retval;
	switch (vstup)
	{
	case 0: retval = cidloLoz.readHumidity() * 10;break;
	case 1: retval = cidloPokJ.readHumidity() * 10;break;
	case 2: retval = cidloPrac.readHumidity() * 10;break;
	case 3: retval = cidloKoup.readHumidity() * 10;break;
	case 4: retval = cidloVenk.readHumidity() * 10;break;
	case 5: retval = cidloSatna.readHumidity() * 10;break;
	case 6: retval = cidloObyv.readHumidity() * 10;break;
	case 7: retval = cidloTech.readHumidity() * 10;break;
	case 8: retval = cidloZz.readHumidity() * 10;break;
		//case 9: retval = cidlo1.readTemperature()*10; break;   
	}//*/
	//Serial.println(retval);
	return retval;
}

/*
ziska cas z modulu DS3231
urci aktualni nastavenou teplotu z tabulky termostatu
*/
void ctiCas()
{
	uint16_t tSrc;
	uint8_t den;
	
	DS3231_get(&dateTime);
	/*
	char buff[30];
	sprintf(buff, "%u.%u.%u  %u:%u:%u", dateTime.mday, dateTime.mon, dateTime.year, dateTime.hour, dateTime.min, dateTime.sec);
	Serial.println(buff);

	//*/
	tSrc = dateTime.min + (dateTime.hour * 60);
	den = dateTime.wday;
   // Serial.println(tSrc);
	//povoleni vystupu pro topeni z FVE
	Fve(tSrc);


	for (int i = 0;i<POCET_CIDEL;i++)
	{
		uint16_t t1 = sekce[i].tables[den].cas1;
		uint16_t t2 = sekce[i].tables[den].cas2;
		uint16_t t3 = sekce[i].tables[den].cas3;
		uint16_t t4 = sekce[i].tables[den].cas4;
		uint16_t t5 = sekce[i].tables[den].cas5;

		if (tSrc>=t1&&tSrc<t2)sekce[i].currentTemp = sekce[i].tables[den].teplota1;
		else if (tSrc >= t2&&tSrc<t3)sekce[i].currentTemp = sekce[i].tables[den].teplota2;
		else if (tSrc >= t3&&tSrc<t4)sekce[i].currentTemp = sekce[i].tables[den].teplota3;
		else if (tSrc >= t4&&tSrc<t5)sekce[i].currentTemp = sekce[i].tables[den].teplota4;
		else if (tSrc >= t5&&tSrc<1440)sekce[i].currentTemp = sekce[i].tables[den].teplota5;

/*
Serial.println(sekce[i].tables[den].cas1);
Serial.println(sekce[i].tables[den].cas2);
Serial.println(sekce[i].tables[den].cas3);
Serial.println(sekce[i].tables[den].cas4);
Serial.println(sekce[i].tables[den].cas5);
//*/
	 
	 
//*/
	}

	 /*test
	 snprintf(buff, 30, "\n%d.%02d.%02d %02d:%02d:%02d", dateTime.year,
	 dateTime.mon, dateTime.mday, dateTime.hour, dateTime.min, dateTime.sec);

	 Serial.println(buff);
	 Serial.println(casVminutach);
	 //end test */
//*/
	for (size_t i = 0; i < 5; i++)
	{
		if (spin[i].manEnable == false)
		{
			if ((tSrc >= spin[i].t1Zap&&tSrc < spin[i].t1Vyp) ||
				(tSrc >= spin[i].t2Zap&&tSrc < spin[i].t2Vyp))
			{
				spin[i].vystup_new = on;
			}
			else spin[i].vystup_new = off;

			if (spin[i].vystup_new != spin[i].vystup_prew)
			{
				spin[i].vystup_prew = spin[i].vystup_new;

				digitalWrite(spin[i].outPin, spin[i].vystup_new);
			//test
//Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
//Serial.println(spin[i].vystup_new);
//Serial.println(spin[i].outPin);
			//end test // 
			}
		}

	}
//*/
		//fve obyv
		if (tSrc >= enableFveObyvak.timeStart&&tSrc < enableFveObyvak.timeStop)
		{
			if (enableFveObyvak.enable == true)
			{
              if(sekce[obyvak].teplotaAkt<enableFveObyvak.temp)enableFveObyvak.vystup_new = on;
			  else enableFveObyvak.vystup_new = off;
			}
				
			//else enableFveObyvak.vystup_new = off;
		}
		else enableFveObyvak.vystup_new = off;
 
		if (enableFveObyvak.vystup_new != enableFveObyvak.vystup_prew)
		{
			enableFveObyvak.vystup_prew = enableFveObyvak.vystup_new;

			digitalWrite(enableFveObyvak.outPin, enableFveObyvak.vystup_new);

		}
		//fve zim zahr
		if (tSrc >= enableFveZimZahrada.timeStart&&tSrc < enableFveZimZahrada.timeStop)
		{
			if (enableFveZimZahrada.enable == true)
			{
				if (sekce[z_zahrada].teplotaAkt<enableFveZimZahrada.temp)enableFveZimZahrada.vystup_new = on;
				else enableFveZimZahrada.vystup_new = off;
			}
				//enableFveZimZahrada.vystup_new = on;
			//else enableFveZimZahrada.vystup_new = off;
		}
		else enableFveZimZahrada.vystup_new = off;

		if (enableFveZimZahrada.vystup_new != enableFveZimZahrada.vystup_prew)
		{
			enableFveZimZahrada.vystup_prew = enableFveZimZahrada.vystup_new;

			digitalWrite(enableFveZimZahrada.outPin, enableFveZimZahrada.vystup_new);
		}
		//fve koupelna
		if (tSrc >= enableFveKoupelna.timeStart&&tSrc < enableFveKoupelna.timeStop)
		{
				if (enableFveKoupelna.enable == true)
			{
				if (sekce[koupelna].teplotaAkt<enableFveKoupelna.temp)enableFveKoupelna.vystup_new = on;
				else enableFveKoupelna.vystup_new = off;
				}
				enableFveKoupelna.vystup_new = on;
			//else enableFveKoupelna.vystup_new = off;
		}
		else enableFveKoupelna.vystup_new = off;

		if (enableFveKoupelna.vystup_new != enableFveKoupelna.vystup_prew)
		{
			enableFveKoupelna.vystup_prew = enableFveKoupelna.vystup_new;

			digitalWrite(enableFveKoupelna.outPin, enableFveKoupelna.vystup_new);
		}
	
}

/*
porovna aktualni teploty s nastavenymi
*/
void porovnejNastavene()
{
	for (int i = 0;i<POCET_CIDEL;i++)
	{
		/*
		Serial.print(*sekce[i].currentTempAdjPtr);
		Serial.print("  ");
		Serial.println(sekce[i].teplotaAkt);
		*/
		
		if (sekce[i].blokovani == true ) { continue; }

		else if (sekce[i].teplotaAkt>=sekce[i].currentTemp)sekce[i].vystup_new = off;
		else if (sekce[i].teplotaAkt<sekce[i].currentTemp - sekce[i].offset)sekce[i].vystup_new = on;
		
		if (sekce[i].vystup_new != sekce[i].vystup_prew)
		{
			sekce[i].vystup_prew = sekce[i].vystup_new;

			digitalWrite(sekce[i].outPin, sekce[i].vystup_new);
			/*/test
			Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
			Serial.println(sekce[i].vystup_new);
			Serial.println(sekce[i].outPin);
			//end test  */
		}
		

	}
}


/*
inicializace cidel DHT 22
*/
void sensorInit()
{
	cidloSatna.begin();
	cidloZz.begin();
	cidloObyv.begin();
	cidloKoup.begin();
	cidloVenk.begin();
	cidloPrac.begin();
	cidloLoz.begin();
	cidloPokJ.begin();
	cidloTech.begin();
	// cidloX.begin();*/
}

/*
ulozi nastaveny cas do eeprom tabulky termostatu
*/
void ulozCas(uint16_t cas, Sekce *sekce, Ee_tab_offset_enum casovy_usek, uint8_t den)
{
	//ukazatel do flash
	uint16_t *ptr = &sekce->tables[den].cas1;
	ptr += casovy_usek;
	*ptr = cas;
	//ukazatel do eeprom 
	int eeprom_adr = sekce->ee_adr + tables_en + casovy_usek + (den*sizeof(TimeTempTable));
	EEPROM.put(eeprom_adr, cas);
	//test
	// uint16_t _cas;
	// EEPROM.get(eeprom_adr,_cas);
	// Serial.println(*ptr);
	// Serial.println(eeprom_adr);
	// Serial.println(_cas);
	//end test
}


/*
ulozi nastavenou teplotu do eeprom tabulky termostatu
*/
void ulozTeplotu(uint16_t teplota, Sekce *sekce, Ee_tab_offset_enum teplota_pro_usek, uint8_t den)
{
	uint16_t *ptr = &sekce->tables[den].teplota1;
	ptr += teplota_pro_usek;
	*ptr = teplota;
	int eeprom_adr = sekce->ee_adr + tables_en + teplota_pro_usek + (den*sizeof(TimeTempTable));
	EEPROM.put(eeprom_adr, teplota);
	/*//test
	 uint16_t _teplota;
     EEPROM.get(eeprom_adr,_teplota);
	 Serial.println(*ptr);
	 Serial.println(eeprom_adr);
	 Serial.println(_teplota);  
	//*/
}




/*
po resetu nacte data ulozena v eeprom
*/
void restoreData()
{
	uint16_t eeprom_adr;

	for (int i = 0;i<POCET_CIDEL;i++)
	{
		eeprom_adr = sekce[i].ee_adr;

		//ovladani
		EEPROM.get(eeprom_adr + blokovani_en, sekce[i].blokovani);
		//Serial.println(sekce[i].blokovani);
		//offset
		EEPROM.get(eeprom_adr + offset_en, sekce[i].offset);
		//nazev
		//EEPROM.get(eeprom_adr + nazev_en, sekce[i].nazev);
		//out pin
		//EEPROM.get(eeprom_adr + outPin_en, sekce[i].outPin);


		EEPROM.get(eeprom_adr + tables_en, sekce[i].tables[0]);
		EEPROM.get(eeprom_adr + (tables_en + sizeof(TimeTempTable)), sekce[i].tables[1]);
		EEPROM.get(eeprom_adr + tables_en + (2 * sizeof(TimeTempTable)), sekce[i].tables[2]);
		EEPROM.get(eeprom_adr + tables_en + (3 * sizeof(TimeTempTable)), sekce[i].tables[3]);
		EEPROM.get(eeprom_adr + tables_en + (4 * sizeof(TimeTempTable)), sekce[i].tables[4]);
		EEPROM.get(eeprom_adr + tables_en + (5 * sizeof(TimeTempTable)), sekce[i].tables[5]);
		EEPROM.get(eeprom_adr + tables_en + (6 * sizeof(TimeTempTable)), sekce[i].tables[6]);

		
	}
    
	// FVE
	eeprom_adr = enableFveObyvak.ee_adr;
	EEPROM.get(eeprom_adr, enableFveObyvak);
	eeprom_adr = enableFveZimZahrada.ee_adr;
	EEPROM.get(eeprom_adr, enableFveZimZahrada);
	eeprom_adr = enableFveKoupelna.ee_adr;
	EEPROM.get(eeprom_adr, enableFveKoupelna);

	//spinacky
	for (size_t i = 0; i < 5; i++)
	{
		eeprom_adr = spin[i].ee_adr;
		EEPROM.get(eeprom_adr, spin[i]);
	}

}

/*
nastaveni funkci I/O arduina
*/
void SettingIO()
{
	//vstupy cidel
	sekce[0].inPin = 41;
	sekce[1].inPin = 43;
	sekce[2].inPin = 39;
	sekce[3].inPin = 35;
	sekce[4].inPin = 37;
	sekce[5].inPin = 29;
	sekce[6].inPin = 33;
	sekce[7].inPin = 45;
	sekce[8].inPin = 31;

/*	pinMode(41, INPUT_PULLUP);
	pinMode(43, INPUT_PULLUP);
	pinMode(39, INPUT_PULLUP);
	pinMode(35, INPUT_PULLUP);
	pinMode(37, INPUT_PULLUP);
	pinMode(29, INPUT_PULLUP);
	pinMode(33, INPUT_PULLUP);
	pinMode(45, INPUT_PULLUP);
	pinMode(31, INPUT_PULLUP);*/

	//vystupy topeni
	sekce[loznice].outPin = 34;//29;loznice
	pinMode(sekce[loznice].outPin, OUTPUT);
	sekce[pokoj].outPin = 36;//31;pokoj j
	pinMode(sekce[pokoj].outPin, OUTPUT);
	sekce[pracovna].outPin = 32;//33;pracovna
	pinMode(sekce[pracovna].outPin, OUTPUT);
	sekce[koupelna].outPin = 28;//35;koupelna
	pinMode(sekce[koupelna].outPin, OUTPUT);
	//sekce [4] nema vystup venkovni
	sekce[satna].outPin = 22;//37;satna
	pinMode(sekce[satna].outPin, OUTPUT);
	sekce[obyvak].outPin = 26;//39;obyvak
	pinMode(sekce[obyvak].outPin, OUTPUT);
	sekce[techMistnost].outPin = 30;//41;tech mistnost
	pinMode(sekce[techMistnost].outPin, OUTPUT);
	sekce[zahrada].outPin = 24;//43;z. zahrada
	pinMode(sekce[zahrada].outPin, OUTPUT);
	//pridane vystupy pro ovladani ssr od FVE
	enableFveObyvak.outPin = 40;
	pinMode(enableFveObyvak.outPin, OUTPUT);
	enableFveZimZahrada.outPin = 38;
	pinMode(enableFveZimZahrada.outPin, OUTPUT);
	enableFveKoupelna.outPin = 42;
	pinMode(enableFveKoupelna.outPin, OUTPUT);

	//dobij baterky
	spin[0].outPin = 44;
	pinMode(spin[0].outPin, OUTPUT);
	//S1 - ovladani stykace
	spin[1].outPin = 46;
	pinMode(spin[1].outPin, OUTPUT);
	//S2 - ovladani stykace
	spin[2].outPin = 48;
	pinMode(spin[2].outPin, OUTPUT);
	//S3 - ovladani stykace
	spin[3].outPin = 47;
	pinMode(spin[3].outPin, OUTPUT);
	//S4 - ovladani stykace
	spin[4].outPin = 49;
	pinMode(spin[4].outPin, OUTPUT);
//	Serial.println(spin[3].outPin);
//	Serial.println(spin[4].outPin);
	//vypnuti zlute ledky na arduinu
	pinMode(13, OUTPUT);
	digitalWrite(13, LOW);

	// disable Ethernet chip
	pinMode(10, OUTPUT);
	digitalWrite(10, HIGH);

	pinMode(NOCNI_PROUD, INPUT_PULLUP);//vstup HDO signal
}

/*
prirazeni adres promennych v eeprom 
*/
void ee_adrInit()
{
	uint16_t ptrToEe;
	for (int i = 0;i<POCET_CIDEL;i++)
	{
		ptrToEe=i*sizeof(Sekce);
		sekce[i].ee_adr = ptrToEe;
		// if(i==0)Serial.println("0");
		//   Serial.println(sekce[i].ee_adr);
	}
	ptrToEe+= sizeof(Sekce);
	//Serial.println("fve");
		enableFveObyvak.ee_adr = ptrToEe;
		//Serial.println(enableFveObyvak.ee_adr);
		//Serial.println(ptrToEe);
		ptrToEe += sizeof(EnableFve);
		enableFveZimZahrada.ee_adr = ptrToEe;
		//Serial.println(enableFveZimZahrada.ee_adr);
		//Serial.println(ptrToEe);
		ptrToEe += sizeof(EnableFve);
		enableFveKoupelna.ee_adr = ptrToEe;
		//Serial.println(enableFveKoupelna.ee_adr);
		//Serial.println(ptrToEe);
	

	ptrToEe += sizeof(EnableFve);
	//Serial.print("spin");Serial.print(sizeof(Spinacky));
	for (int i = 0;i<5;i++)
	{
		 
		spin[i].ee_adr = ptrToEe;
        ptrToEe +=sizeof(Spinacky);
       // Serial.println(spin[i].ee_adr);
       
		 //if(i==0)Serial.println("0");
		  
       
	}
}



/*
defaultni hodnoty promennych - pouze pri prvnim nahrani programu
*/ 
void defaultValues()
{
	int eeprom_adr;
	for (int i = 0;i<POCET_CIDEL;i++)
	{
		sprintf(sekce[i].nazev, "cidlo %u", i);

		eeprom_adr = sekce[i].ee_adr + nazev_en;
		//default nazev
		EEPROM.put(eeprom_adr, sekce[i].nazev);
		//default offset
		eeprom_adr = sekce[i].ee_adr + offset_en;
		EEPROM.put(eeprom_adr, 5);
		//default ovladani
		eeprom_adr = sekce[i].ee_adr + blokovani_en;
		EEPROM.put(eeprom_adr, automat);
		//default offset
		//eeprom_adr = sekce[i].ee_adr + outPin_en;
		//EEPROM.put(eeprom_adr, 35 + i);  //vystupy   22-30
		for (int j = 0;j<7;j++)
		{
			ulozCas(0, &sekce[i], cas1_en, j);
			ulozTeplotu(200 + j, &sekce[i], tep1_en, j);
			ulozCas(480 + i, &sekce[i], cas2_en, j);
			ulozTeplotu(220 + j, &sekce[i], tep2_en, j);
			ulozCas(600 + i, &sekce[i], cas3_en, j);
			ulozTeplotu(180 + j, &sekce[i], tep3_en, j);
			ulozCas(1080 + i, &sekce[i], cas4_en, j);
			ulozTeplotu(210 + j, &sekce[i], tep4_en, j);
			ulozCas(1320 + i, &sekce[i], cas5_en, j);
			ulozTeplotu(180 + j, &sekce[i], tep5_en, j);


		}
	}
	eeprom_adr = enableFveZimZahrada.ee_adr;
	enableFveZimZahrada.timeStart = 300;
	enableFveZimZahrada.timeStop = 1200;
	enableFveZimZahrada.temp = 300;
	enableFveZimZahrada.enable = true;
	EEPROM.put(eeprom_adr, enableFveZimZahrada);

	eeprom_adr = enableFveObyvak.ee_adr;
	enableFveObyvak.timeStart = 300;
	enableFveObyvak.timeStop = 1200;
	enableFveObyvak.temp = 300;
	enableFveObyvak.enable = true;
	EEPROM.put(eeprom_adr, enableFveObyvak);

	eeprom_adr = enableFveKoupelna.ee_adr;
	enableFveKoupelna.timeStart = 300;
	enableFveKoupelna.timeStop = 1200;
	enableFveKoupelna.temp = 300;
	enableFveKoupelna.enable = true;
	EEPROM.put(eeprom_adr, enableFveKoupelna);

	for (size_t i = 0; i < 5; i++)
	{
		spin[i].t1Zap = 300;
		spin[i].t1Vyp = 600;
		spin[i].t2Zap = 1000;
		spin[i].t2Vyp = 1200;
		EEPROM.put(spin[i].ee_adr, spin[i]);
	}
}

void test()
{
	int sw = sizeof(int);
	//Serial.print("xxxxx");Serial.println(sw);
	sekce[4].teplotaAkt = 240;
	sekce[4].vlhkostAkt = 500;
	
}
#pragma endregion

#pragma region DS18B20
/*
inicializace cidel DS18B20
*/
void DallasInit()
{
	sensors.begin();

	// Grab a count of devices on the wire
	numberOfDevices = sensors.getDeviceCount();
	for (int i = 0;i<numberOfDevices; i++)
	{
		// Search the wire for address
		sensors.getAddress(tempDeviceAddress, i);
		sensors.getAddress(addr[i], i);
		// set the resolution to TEMPERATURE_PRECISION bit (Each Dallas/Maxim device is capable of several different resolutions)
		sensors.setResolution(tempDeviceAddress, TEMPERATURE_PRECISION);
		Serial.print(addr[i][0],16);
		Serial.print(addr[i][1], 16);
		Serial.print(addr[i][2], 16);
		Serial.print(addr[i][3], 16);
		Serial.print(addr[i][4], 16);
		Serial.print(addr[i][5], 16);
		Serial.print(addr[i][6], 16);
		Serial.println(addr[i][7], 16);
	}
}

#pragma endregion
#pragma region WEB


///////////////////////////////////////////////////////////////
//WEB
///////////////////////////////////////////////////////////////
uint8_t char_index;
boolean newPage = true, firstPage = true;

/*
komunikace pres ethernet
*/
void webServer()
{
	///////////////////////////////////////////////////////////////////////////
	EthernetClient client = server.available();  // try to get client
    
	if (client) {  // got client?
		boolean currentLineIsBlank = true;
		char ch;
		
		// Serial.println("step1");
		while (client.connected()) {
			if (client.available()) {   // client data available to read
				char c = client.read(); // read 1 byte (character) from client
										// limit the size of the stored received HTTP request
										// buffer first part of HTTP request in HTTP_req array (string)
										// leave last element in array as 0 to null terminate string (REQ_BUF_SZ - 1)
				if (req_index < (REQ_BUF_SZ - 1))
				{

					HTTP_req[req_index] = c;          // save HTTP request character
					req_index++;
				}
				// last line of client request is blank and ends with \n
				// respond to client only after last line received
				if (c == '\n' && currentLineIsBlank)
				{
					// send a standard http response header
					client.println("HTTP/1.1 200 OK");
			Serial.print(HTTP_req);
					String httpString = String(HTTP_req);
					if (StrContains(HTTP_req, "ajax_")) {
					//	Serial.println("ok resp");

						client.println();
						//  SetLEDs();
						// send XML file containing input states
						if (StrContains(HTTP_req, "aktVal"))
						{
							//Serial.println("OKVAL");
							XML_response_srcVal(client);
						}
						else if (StrContains(HTTP_req, "fveVal"))
						{
							//Serial.println("OKVAL");
							XML_response_FVE(client);
						}
						else if (StrContains(HTTP_req, "setVal"))
						{
							//Serial.println("OKSET");
							ch = httpString.charAt(httpString.indexOf('$') + 1);
							char_index = ch - 0x30;
							XML_response_tblVal(client, char_index);
						}

						else if (StrContains(HTTP_req, "save"))
						{
							//Serial.println("ok1");
							if (StrContains(HTTP_req, "Fve"))
							{
								//Serial.println("ok11");
                              SaveFveValues(httpString);
							  
							}
							else if (StrContains(HTTP_req, "Spin"))
							{
								SaveSpinValues(httpString);

							}
							SaveValues(httpString);
						}
						else if (StrContains(HTTP_req, "switch"))
						{
							if (StrContains(HTTP_req, "FVE"))
							{
								SwitchFve(HTTP_req);
							}
							else if (StrContains(HTTP_req, "SPIN"))
							{
								SwitchManSpin(HTTP_req);
							}
							else if (StrContains(HTTP_req, "MAN"))
							{
								SwitchSpin(HTTP_req);
							}
							else SwitchHeat(HTTP_req);
							XML_response_srcVal(client);
						}
						else if (StrContains(HTTP_req, "HDO"))
						{
							SwitchHdo(HTTP_req);
							XML_response_srcVal(client);
						}

						else if (StrContains(HTTP_req, "synchro"))
						{
						TimeSynchro(HTTP_req);
							
						}
					}
					else  {  // web page request
							// send rest of HTTP headerif(newPage)
						//firstPage = false;
						//newPage = false;
						SendPage(client);
					}
					// display received HTTP request on serial port
					//Serial.print(HTTP_req);
					// reset buffer index and all buffer elements to 0
					req_index = 0;
					StrClear(HTTP_req, REQ_BUF_SZ);
					break;
				}
				// every line of text received from the client ends with \r\n
				if (c == '\n') {
					// last character on line of received text
					// starting new line with next character read
					currentLineIsBlank = true;
				}
				else if (c != '\r') {
					// a text character was received from client
					currentLineIsBlank = false;
				}
			} // end if (client.available())
		} // end while (client.connected())
		delay(3);      // give the web browser time to receive the data
		client.stop(); // close the connection
	} // end if (client)
	  ////////////////////////////////////////////////////////////////////////////////
}


/*
odeslani html stranky
*/ 
void SendPage(EthernetClient client)
{
	//Serial.println("sendPage++++");
	client.println("Content-Type: text/html");
	//client.println("Connection: keep-alive");
	client.println();
	// send web page
	//if (htmlFile == valueHtml)
	//{
	webFile = SD.open("topeni.htm");
	//Serial.println("topeniHtml");
	//}

	//else
	//{
	//webFile = SD.open("settings.htm");
	//Serial.println("setting");
	//}
	// open web page file
	if (webFile)
	{
	while (webFile.available()) {
	client.write(webFile.read()); // send web page to client

	}
	webFile.close();
	//Serial.println("webFile.close");
	}
}

/*
synchronizace casu s PC
*/
void TimeSynchro(String str)
{
	//struct ts dateTime1;
	//Serial.println(str + "!!!");
	int index = str.indexOf('$') + 1;
	String _str = str.substring(index, index + 24);
	char pomStr[25];
	_str.toCharArray(pomStr, 23);
	ValueStruct vs = RozdelString(pomStr, ':');
	dateTime.mday = vs.a.toInt();
	dateTime.mon = vs.b.toInt()+1;
	dateTime.year = vs.c.toInt();
	dateTime.hour = vs.d.toInt();
	dateTime.min = vs.e.toInt();
	dateTime.sec = vs.f.toInt();
	dateTime.wday = vs.g.toInt();
	DS3231_set(dateTime);
/*
	//Serial.print(vs.a + vs.b + vs.c);
	//Serial.print(vs.d + vs.e + vs.f + "xx");
	char buff[35];
	//sprintf(buff, "%u.%u.%u  %u:%u:%u", dateTime1.mday, dateTime1.mon, dateTime1.year, dateTime1.hour, dateTime1.min, dateTime1.sec);
	//Serial.println(buff);
	sprintf(buff, "%u.%u.%u  %u:%u:%u  %u", dateTime.mday, dateTime.mon, dateTime.year, dateTime.hour, dateTime.min, dateTime.sec,dateTime.wday);
	Serial.println(buff);

	//*/
}

/*
ulozeni hodnot tabulky termostatu  do eeprom
*/ 
void SaveValues(String str)
{
	char pomArray[35];

	//cislo sekce
	char ch = str.charAt(str.indexOf('$')+1);
	uint8_t sect = ch - 0x30;
	


	String pomTepl, pomCas;
	unsigned int start, end;
	start = str.indexOf('$') + 2;
	end = start + 2;
	//den v tydnu
    String den= str.substring(start, end);
	start = str.indexOf('C') + 2;
	end=start+ 31;
	//casy
	pomCas = str.substring(start, end);
	start = str.indexOf("T_") + 2;
	end = str.indexOf("&") ;
	//teploty
	pomTepl = str.substring(start, end);
   // Serial.println(pomCas);
//	Serial.println(pomTepl);

    pomCas.toCharArray(pomArray, 35, 0);
	ValueStruct vs = RozdelString(pomArray, ';');
	pomTepl.toCharArray(pomArray, 35, 0);
	ValueStruct vs2 = RozdelString(pomArray, ';');
  //  Serial.println(den);
	int _den = KteryDen(den);
//Serial.println(_den);
	 //_cas = TimeConvertToInt(vs.a);	
	//sekce[sect].tables[_den].cas1 = _cas;
	//ulozCas(_cas, &sekce[sect], cas1_en, _den);
	int _cas =TimeConvertToInt(vs.b);
	sekce[sect].tables[_den].cas2 = _cas;
	ulozCas(_cas, &sekce[sect], cas2_en, _den);
	_cas=TimeConvertToInt(vs.c);
	sekce[sect].tables[_den].cas3 = _cas;
	ulozCas(_cas, &sekce[sect], cas3_en, _den);
	_cas=TimeConvertToInt(vs.d);
	sekce[sect].tables[_den].cas4 = _cas;
	ulozCas(_cas, &sekce[sect], cas4_en, _den);
	_cas=TimeConvertToInt(vs.e);
	sekce[sect].tables[_den].cas5 = _cas;
	ulozCas(_cas, &sekce[sect], cas5_en, _den);
	//teploty
	float pomFl = vs2.a.toFloat()* 10;
	sekce[sect].tables[_den].teplota1 = (int)pomFl ;
	ulozTeplotu((int)pomFl, &sekce[sect], tep1_en, _den);
	pomFl=vs2.b.toFloat() * 10;
	sekce[sect].tables[_den].teplota2 = (int)pomFl;
	ulozTeplotu((int)pomFl, &sekce[sect], tep2_en, _den);
	pomFl =vs2.c.toFloat() * 10;
	sekce[sect].tables[_den].teplota3 = (int)pomFl;
	ulozTeplotu((int)pomFl, &sekce[sect], tep3_en, _den);
	pomFl =vs2.d.toFloat() * 10;
	sekce[sect].tables[_den].teplota4 = (int)pomFl;
	ulozTeplotu((int)pomFl, &sekce[sect], tep4_en, _den);
	pomFl =vs2.e.toFloat() * 10;
	sekce[sect].tables[_den].teplota5 = (int)pomFl;
	ulozTeplotu((int)pomFl, &sekce[sect], tep5_en, _den);

    restoreData();
	/*

	
	Serial.println(vs.a);
	Serial.println(vs.b);
	Serial.println(vs.c);
	Serial.println(vs.d);
	Serial.println(vs.e);

	Serial.println(sekce[sect].tables[_den].teplota1);
	Serial.println(sekce[sect].tables[_den].teplota2);
	Serial.println(sekce[sect].tables[_den].teplota3);
	Serial.println(sekce[sect].tables[_den].teplota4);
	Serial.println(sekce[sect].tables[_den].teplota5);

	Serial.println(sekce[sect].tables[_den].cas1);
	Serial.println(sekce[sect].tables[_den].cas2);
	Serial.println(sekce[sect].tables[_den].cas3);
	Serial.println(sekce[sect].tables[_den].cas4);
	Serial.println(sekce[sect].tables[_den].cas5);
	//*/
	
}

/*
ulozeni hodnot pro moznost topeni z FVE
*/
void SaveFveValues(String str)
{
	char charArr[100];
	//Serial.println("ok3");
	str.toCharArray(charArr,0);
	//Serial.println(charArr);
	int start;
	int stop ;
	uint16_t eeadr;
	start = str.indexOf(';')+1;
	stop = start + 5;


		String cas = str.substring(start, stop);
  //Serial.println(cas);
		enableFveKoupelna.timeStart = TimeConvertToInt(cas);
		start = stop + 1;
		stop = start + 5;
	    cas = str.substring(start, stop);
  //Serial.println(cas);
		enableFveKoupelna.timeStop = TimeConvertToInt(cas);
		start = stop + 1;
		stop = start + 2; 
 
		String tepl = str.substring(start, stop);
  //Serial.println(tepl);
		enableFveKoupelna.temp = tepl.toInt()*10;
		char ch=str.charAt(stop + 1)-0x30;
		enableFveKoupelna.enable = (bool)ch;
 //Serial.println(enableFveObyvak.enable);
		eeadr = enableFveKoupelna.ee_adr;
		EEPROM.put(eeadr, enableFveKoupelna);

		start = stop + 4;
		stop = start + 5;
	    cas = str.substring(start, stop);
  //Serial.println(cas);
		enableFveObyvak.timeStart = TimeConvertToInt(cas);
		start = stop + 1;
		stop = start + 5;
		cas = str.substring(start, stop);
  //Serial.println(cas);
		enableFveObyvak.timeStop = TimeConvertToInt(cas);
		start = stop + 1;
		stop = start + 2;
	    tepl = str.substring(start, stop);
  //Serial.println(tepl);
		enableFveObyvak.temp = tepl.toInt()*10;
	    ch = str.charAt(stop + 1) - 0x30;
		enableFveObyvak.enable = (bool)ch;
  //Serial.println(enableFveZimZahrada.enable);
		eeadr = enableFveObyvak.ee_adr;
		EEPROM.put(eeadr, enableFveObyvak);

		start = stop + 3;
		stop = start + 5;
		cas = str.substring(start, stop);
 //Serial.println(cas);
		enableFveZimZahrada.timeStart = TimeConvertToInt(cas);
		start = stop + 1;
		stop = start + 5;
		cas = str.substring(start, stop);
 //Serial.println(cas);
		enableFveZimZahrada.timeStop = TimeConvertToInt(cas);
		start = stop + 1;
		stop = start + 2;
		tepl = str.substring(start, stop);
 //Serial.println(tepl);
		enableFveZimZahrada.temp = tepl.toInt() * 10;
		ch = str.charAt(stop + 1) - 0x30;
		enableFveZimZahrada.enable = (bool)ch;
		Serial.println("eeeeeEE");
		eeadr = enableFveKoupelna.ee_adr;
        EEPROM.put(eeadr, enableFveKoupelna);
 //Serial.println(enableFveKoupelna.ee_adr);
		eeadr = enableFveObyvak.ee_adr;
		EEPROM.put(eeadr, enableFveObyvak);
 //Serial.println(enableFveObyvak.ee_adr);
		eeadr = enableFveZimZahrada.ee_adr;
		EEPROM.put(eeadr, enableFveZimZahrada);
 //Serial.println(enableFveZimZahrada.ee_adr);


}



/*

*/
void SaveSpinValues(String str)
{
	//Serial.println(str);
	char c = str.charAt(str.indexOf(';') - 1);
	int vystup = atoi(&c);
	char pomArray[35];
	unsigned int start, end;
	start = str.indexOf(';');
	end = start + 24;
	String  pomCas = str.substring(start, end);
	//Serial.println(pomCas);
	pomCas.toCharArray(pomArray, 35, 0);
	ValueStruct vs = RozdelString(pomArray, ';');
	spin[vystup].t1Zap = TimeConvertToInt(vs.a);
	spin[vystup].t1Vyp = TimeConvertToInt(vs.b);
	spin[vystup].t2Zap = TimeConvertToInt(vs.c);
	spin[vystup].t2Vyp = TimeConvertToInt(vs.d);
	EEPROM.put(spin[vystup].ee_adr, spin[vystup]);

	//Serial.println(spin[vystup].ee_adr);
}
/*
konverze text. nazvu dne do hodnoty int 0-6 (0=nedele)
*/
int KteryDen(String str)
{
	int retval
		= (str == "po") ? 1
		: (str == "ut") ? 2
		: (str == "st") ? 3
		: (str == "ct") ? 4
		: (str == "pa") ? 5
		: (str == "so") ? 6
		: (str == "ne") ? 0 :7 ;
	return retval;
}

/*
konverze casoveho udaje v textu do hodnoty int - prevedeno na pocet minut
*/
uint16_t TimeConvertToInt(String str)
{
	//Serial.println(str);
	String strHod = str.substring(0, 2);
   // Serial.println(strHod);
	int hod = strHod.toInt();
	String strMin = str.substring(3, 5);
	//Serial.println(strMin);
	int min = strMin.toInt();
	//Serial.println(strMin);
	return (hod * 60) + min;
}

/*
rozdeleni textu "str" na useky oddelene pomoci "separator"u
vraci strukturu stringu
*/
 ValueStruct RozdelString(char* str,char separator)
{
	ValueStruct vs;
	
	int is[8];
	String arrStr[7];
	uint8_t cnt=0;
	uint8_t cnt1 = 0;
	String str1 = str;
	int len = strlen(str);
	for (int i = 0;i < len; i++)	
	{
		if (str1[i] == separator)
		{
			is[cnt] = i;
			cnt++;
		}
	}

	vs.a = str1.substring(is[0]+1, is[1]);
	vs.b = str1.substring(is[1]+1, is[2]);
	vs.c = str1.substring(is[2]+1, is[3]);
	vs.d = str1.substring(is[3]+1, is[4]);
	vs.e = str1.substring(is[4]+1, is[5]);
	vs.f = str1.substring(is[5] + 1, is[6]);
	vs.g = str1.substring(is[6] + 1, is[7]);

		return vs;
}

 /*
 pomoci tlacitka na UI blokuje/povoluje spinani topeni jednotlivych sekci
 */
 void SwitchHeat(String str)
 {
	 int index = str.indexOf('$');
	 //cislo sekce
	 char ch = str.charAt(index + 1);
	 uint8_t sect = ch - 0x30;
	 String onOff = str.substring(index - 2, index);
	 uint16_t eeadr = sekce[sect].ee_adr + blokovani_en;
	// Serial.println(eeadr);
	 if (onOff == "on")//&&hdo_temp==0
	 {
		 sekce[sect].blokovani = false;
		 tmp_blok[sect] = sekce[sect].blokovani;
		 sekce[sect].vystup_new = on;
		 digitalWrite(sekce[sect].outPin, sekce[sect].vystup_new);

		 EEPROM.put(eeadr, sekce[sect].blokovani);
	 }
	  else if (onOff == "ff")
	  {
		  digitalWrite(sekce[sect].outPin,LOW);
		//  sekce[sect].vystup_new = off;
		  sekce[sect].blokovani = true;
		  tmp_blok[sect] = sekce[sect].blokovani;
		  EEPROM.put(eeadr, sekce[sect].blokovani);
	  }
	// Serial.println(onOff+ sekce[sect].blokovani);
 }

 /*
 
 */
 void SwitchFve(String str)
 {
	 int index = str.indexOf('$');
	 //cislo sekce
	 char ch = str.charAt(index + 1);
	 uint8_t sect = ch - 0x30;
	 String onOff = str.substring(index - 2, index);
	 uint16_t eeadr;
	 EnableFve *ptr;
	 switch (sect)
	 {
	 case 0: eeadr = enableFveObyvak.ee_adr; ptr = &enableFveObyvak; break;
	 case 1: eeadr = enableFveKoupelna.ee_adr; ptr = &enableFveKoupelna; break;
	 case 2: eeadr = enableFveZimZahrada.ee_adr; ptr = &enableFveZimZahrada; break;
	 }
	 // Serial.println(eeadr);
	 if (onOff == "on")//&&hdo_temp==0
	 {
		 ptr->enable = true;		 
		// ptr->vystup_new = on;
		 digitalWrite(ptr->outPin, ptr->vystup_new);
		 EEPROM.put(eeadr+2, ptr->enable);
	 }
	 else if (onOff == "ff")
	 {
		 digitalWrite(ptr->outPin, LOW);
		 ptr->vystup_new = off;
		 ptr->enable = false;
		 EEPROM.put(eeadr + 2, ptr->enable);
	 }
	 //Serial.println("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
	 //Serial.println(eeadr);
	  //Serial.println(onOff+ ptr->enable);
 }

 /*

 */
 void SwitchManSpin(String str)
 {
	 int index = str.indexOf('$');
	 //cislo sekce
	 char ch = str.charAt(index + 1);
	 uint8_t sect = ch - 0x30;
	 String onOff = str.substring(index - 2, index);
	 uint16_t eeadr = spin[sect].ee_adr;
	 //Serial.println(eeadr);
	 if (spin[sect].manEnable == true)
	 {
		if (onOff == "on")//&&hdo_temp==0
		{
			spin[sect].vystup_new = on;
			digitalWrite(spin[sect].outPin, HIGH);
		}
		else if (onOff == "ff")
		{
			spin[sect].vystup_new = off;
			digitalWrite(spin[sect].outPin, LOW);
		}
	 }

	 //Serial.println("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
	// Serial.println(eeadr);
	 //Serial.println(onOff + spin[sect].enable);
 }

 /*

 */
 void SwitchSpin(String str)
 {
	 int index = str.indexOf('$');
	 //cislo sekce
	 char ch = str.charAt(index + 1);
	 uint8_t sect = ch - 0x30;
	 String onOff = str.substring(index - 2, index);
	 uint16_t eeadr = spin[sect].ee_adr;
	 //Serial.println(eeadr);
	 if (onOff == "ff")//&&hdo_temp==0
	 {
		 spin[sect].manEnable = true;
         tmp_spin[sect] = spin[sect].vystup_new;
		 EEPROM.put(eeadr + 2, spin[sect].manEnable);
	 }
	 else if (onOff == "on")
	 {
		spin[sect].manEnable = false;
		
		digitalWrite(spin[sect].outPin, tmp_spin[sect]);
	    EEPROM.put(eeadr + 2, spin[sect].manEnable);
	 }
	 Serial.println("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
	 // Serial.println(eeadr);
	 Serial.println(onOff + spin[sect].manEnable);
 }
 /*
 pomoci tlacitka na UI blokuje/povoluje signal HDO
 */
 void SwitchHdo(String str)
 {
	 
	 int index = str.indexOf('$');

	 String onOff = str.substring(index - 2, index);

	 if (onOff == "on")//&&hdo_temp==0
	 {
		 hdo_rucne = true;
		/* for (int i = 0;i < POCET_CIDEL;i++)
		 {
			 if (i == 4)continue;//sekce 4 venkovni cidlo nema vystup
			 digitalWrite(sekce[i].outPin, LOW);
			 sekce[i].vystup_prew = off;
			 tmp_blok[i] = sekce[i].blokovani;
			 sekce[i].blokovani = true;
		 }*/
	 }
	 else if (onOff == "ff")
	 {
		 hdo_rucne = false;

	 }
	// hdo_rucne_temp = hdo_rucne;
	/*	 for (int i = 0;i < POCET_CIDEL;i++)
		 {
			 if (i == 4)continue;//sekce 4 venkovni cidlo nema vystup

			 sekce[i].blokovani = tmp_blok[i];
			 digitalWrite(sekce[i].outPin, sekce[i].vystup_new);
		 }*/
	 // Serial.println(onOff+ sekce[sect].blokovani);
 }

/*
posle XML document s aktualnimi hodnotami cidel a vystupu
*/
void XML_response_srcVal(EthernetClient cl)
{
	//Serial.println("xml resp");
	int i;                 // used by 'for' loops
	float pom;

	cl.print("<?xml version = \"1.0\" ?>");
	cl.print("<values>");
	//identifikace stranky
	cl.print("<stranka>");
	cl.print("val");
	cl.println("</stranka>");
	//* aktualni teploty
	//String tVal;
	for (i = 0; i < POCET_CIDEL; i++)
	{
		pom = (float)sekce[i].teplotaAkt / 10;
		//if (pom < 0)
		//{
          //pom = fabs(pom);
		  //tVal = "-" + String(pom);
		  //Serial.println(tVal);
		//}
		//else
			//tVal = String(pom);
		cl.print("<teplotaA>");
		cl.print(pom);
		
		cl.println("</teplotaA>");
	}
	uint16_t pomT;
	//* nastavene teploty
	for (i = 0; i < POCET_CIDEL; i++)
	{
		if (i == 4)continue;
		pomT = sekce[i].currentTemp;
		pom = (float)pomT / 10;
		cl.print("<teplotaN>");
		cl.print(pom);
		cl.println("</teplotaN>");
	}

	// aktualni vlhkost
	for (i = 0; i < POCET_CIDEL; i++)
	{
		pom = (float)sekce[i].vlhkostAkt / 10;
		cl.print("<vlhkost>");
		cl.print(pom);
		cl.println("</vlhkost>");
	}

	// aktualni stav vystupu topeni
	EnumSwitch esw;
	for (i = 0; i < POCET_CIDEL; i++)
	{
		if (i == 4)continue;
		if (sekce[i].blokovani == true)esw = disable;
		else esw = sekce[i].vystup_new;
		cl.print("<topeni>");
		cl.print(esw);
		cl.println("</topeni>");
	}

	// aktualni stav prepinace topeni
	bool pom1;
	for (i = 0; i < POCET_CIDEL; i++)
	{ 
		if (i == 4)continue;
		pom1 = sekce[i].blokovani;
		cl.print("<topeniSwitch>");
		cl.print(pom1);
		cl.println("</topeniSwitch>");
	}

	//  teploty v nadrzi 
		pom = (float)teplotaNadrz[1] / 10;
		cl.print("<teplotaNadrz>");
		cl.print(pom);
		cl.println("</teplotaNadrz>");

		pom = (float)teplotaNadrz[2] / 10;
		cl.print("<teplotaNadrz>");
		cl.print(pom);
		cl.println("</teplotaNadrz>");

		pom = (float)teplotaNadrz[6] / 10;
		cl.print("<teplotaNadrz>");
		cl.print(pom);
		cl.println("</teplotaNadrz>");


	cl.print("<teplotaVzt>");
	pom = (float)teplotaNadrz[0] / 10;
	cl.print(pom);
	cl.println("</teplotaVzt>");

	cl.print("<teplotaSklep>");
	pom = (float)teplotaNadrz[3] / 10;
	cl.print(pom);
	cl.println("</teplotaSklep>");

	cl.print("<teplotaSklep>");
	pom = (float)teplotaNadrz[4] / 10;
	cl.print(pom);
	cl.println("</teplotaSklep>");

	cl.print("<teplotaUni>");
	pom = (float)teplotaNadrz[5] / 10;
	cl.print(pom);
	cl.println("</teplotaUni>");

	cl.print("<cidloCO2>");
	cl.print(CO2value);
	cl.println("</cidloCO2>");

	cl.print("<hdo>");
	String str;
	if(hdo_temp==0)str="zapnut";
	else str="vypnut";
    if(hdo_rucne==true)str="zapnut rucne";
	cl.print(str);
	cl.println("</hdo>");
    char tmp[25];
    cl.print("<timedate>");	
	sprintf(tmp, "%02u:%02u:%02u", dateTime.hour, dateTime.min, dateTime.sec);
	cl.print(tmp);
	cl.println("</timedate>");
	cl.print("<timedate>");
	sprintf(tmp, "%u.%u.%u", dateTime.mday, dateTime.mon, dateTime.year);
	cl.print(tmp);
	cl.println("</timedate>");
	cl.print("<timedate>");
	sprintf(tmp, "%u",dateTime.wday);
	cl.print(tmp);
	cl.println("</timedate>");
	cl.print("<fve>");
    if(enableFveKoupelna.enable==false)cl.print("vypnuto trvale");
	else if(enableFveKoupelna.vystup_new == on)cl.print("zapnuto");
	else cl.print("vypnuto");
	
	cl.println("</fve>");
	cl.print("<fve>");
	if (enableFveObyvak.enable == false)cl.print("vypnuto trvale");
	else if (enableFveObyvak.vystup_new == on)cl.print("zapnuto");
	else cl.print("vypnuto");
	cl.println("</fve>");
	cl.print("<fve>");
	if (enableFveZimZahrada.enable == false)cl.print("vypnuto trvale");
	else if (enableFveZimZahrada.vystup_new == on)cl.print("zapnuto");
	else cl.print("vypnuto");
	cl.println("</fve>");

	for (size_t i = 0; i < 5; i++)
	{
		cl.print("<spOut>");
		if (spin[i].manEnable == false)
		{
			if (spin[i].vystup_new == on)cl.print("zapnuto auto");
			else cl.print("vypnuto auto");
		}
		else
		{
			if (spin[i].vystup_new == on)cl.print("zapnuto rucne");
			else cl.print("vypnuto rucne");
		}
		cl.println("</spOut>");
	}

	cl.print("</values>");

	
}

/*
posle XML document s tabulkami tydenniho termostatu pro jednotlive sekce (cidlo)
*/
void XML_response_tblVal(EthernetClient cl, uint8_t cidlo)
{//
	//Serial.println("xml resp222");
	int i, j;                 // used by 'for' loops
	float pom;
	uint16_t *ptr;
	char cas[6];
	//String mistnosti[] = { "Ložnice","Pokoj j.", "Pracovna", "Koupelna", "Chodba", "Obývák", "Tech. místnost", "Zimní zahrada" };

	cl.print("<?xml version = \"1.0\" ?>");
	cl.print("<values>");
	//identifikace stranky
	cl.print("<stranka>");
	cl.print("set");
	cl.println("</stranka>");
	cl.print("<sekce>");
	cl.print(cidlo);
	cl.println("</sekce>");
	uint8_t pp;
	for (j = 0;j<7;j++)
	{
		pp = j;
		if (j == 0)pp = 7;
		ptr = &sekce[cidlo].tables[j].teplota1;
		for (i = 0; i < 5; i++)
		{

			pom = (float)*ptr;
			/*  Serial.print("<temp"+String(j+1)+">");
			Serial.print(pom);
			Serial.println("</temp"+String(j+1)+">");*/
		
			pom /= 10;
			cl.print("<temp" + String(pp) + ">");
			cl.print(pom);
			cl.println("</temp" + String(pp) + ">");
			ptr += 2;
		}
	}


	for (j = 0; j <7; j++)
	{
		pp = j;
		if (j == 0)pp = 7;
		ptr = &sekce[cidlo].tables[j].cas1;
		for (i = 0; i < 5; i++)
		{
			sprintf(cas, "%02u:%02u", *ptr / 60, *ptr % 60);//10,10
															/*   Serial.print("<time"+String(j+1)+">");
															Serial.print(cas);
															Serial.println("</time"+String(j+1)+">");*/
			cl.print("<time" + String(pp) + ">");
			cl.print(cas);
			cl.println("</time" + String(pp) + ">");
			ptr += 2;
		}


	}
	cl.print("</values>");

//	Serial.println("end set resp");
}
// sets every element of str to 0 (clears array)
void StrClear(char *str, char length)
{
	for (int i = 0; i < length; i++) {
		str[i] = 0;
	}
}

// searches for the string sfind in the string str
// returns 1 if string found
// returns 0 if string not found
char StrContains(char *str, char *sfind)
{
	char found = 0;
	char index = 0; 
	char len;

	len = strlen(str);

	if (strlen(sfind) > len) {
		return 0;
	}
	while (index < len) {
		if (str[index] == sfind[found]) {
			found++;
			if (strlen(sfind) == found) {
				return 1;
			}
		}
		else {
			found = 0;
		}
		index++;
	}
	

	return 0;
}

void XML_response_FVE(EthernetClient cl)
{
	///Serial.println("fvexxx");
	char cas[6];
	float tempVal;
	uint16_t ptr;
	uint16_t *addr;
	cl.print("<?xml version = \"1.0\" ?>");
	cl.print("<values>");
	//identifikace stranky
	cl.print("<stranka>");
	cl.print("fve");
	cl.println("</stranka>");
		cl.print("<fveTime>");
			ptr = enableFveKoupelna.timeStart;
			sprintf(cas, "%02u:%02u", ptr / 60, ptr % 60);
			cl.print(cas);
		cl.print("</fveTime>");
		cl.print("<fveTime>");
			ptr = enableFveKoupelna.timeStop;
			sprintf(cas, "%02u:%02u", ptr / 60, ptr % 60);
			cl.print(cas);
		cl.print("</fveTime>");

		cl.print("<fveTime>");
			ptr = enableFveObyvak.timeStart;
			sprintf(cas, "%02u:%02u", ptr / 60, ptr % 60);
			cl.print(cas);
		cl.print("</fveTime>");
		cl.print("<fveTime>");
			ptr = enableFveObyvak.timeStop;
			sprintf(cas, "%02u:%02u", ptr / 60, ptr % 60);
			cl.print(cas);
		cl.print("</fveTime>");

		cl.print("<fveTime>");
			ptr = enableFveZimZahrada.timeStart;
			sprintf(cas, "%02u:%02u", ptr / 60, ptr % 60);
			cl.print(cas);
			cl.print("</fveTime>");
		cl.print("<fveTime>");
			ptr = enableFveZimZahrada.timeStop;
			sprintf(cas, "%02u:%02u", ptr / 60, ptr % 60);
			cl.print(cas);
		cl.print("</fveTime>");





		cl.print("<fveTemp>");
			tempVal = (float)enableFveKoupelna.temp;
			tempVal /= 10;
			int intVal = (int)tempVal;
			cl.print(intVal);
		cl.print("</fveTemp>");

		cl.print("<fveTemp>");
			tempVal = (float)enableFveObyvak.temp;
			tempVal /= 10;
			intVal = (int)tempVal;
			cl.print(intVal);
		cl.print("</fveTemp>");

		cl.print("<fveTemp>");
			tempVal = (float)enableFveZimZahrada.temp;
			tempVal /= 10;
			intVal = (int)tempVal;
			cl.print(intVal);
		cl.print("</fveTemp>");


		cl.print("<fveOutState>");
			if (enableFveKoupelna.vystup_new == on)cl.print("sepnut");
			else cl.print("vypnut");
		cl.print("</fveOutState>");

		cl.print("<fveOutState>");
			if(enableFveObyvak.vystup_new==on)cl.print("sepnut");
			else cl.print("vypnut");			
		cl.print("</fveOutState>");

		cl.print("<fveOutState>");
			if (enableFveZimZahrada.vystup_new == on)cl.print("sepnut");
			else cl.print("vypnut");
		cl.print("</fveOutState>");

		cl.print("<fveEnable>");
			if (enableFveKoupelna.enable == true)cl.print("Povolit");
			else cl.print("Zakazat");
		cl.print("</fveEnable>");

		cl.print("<fveEnable>");
			if (enableFveObyvak.enable == true)cl.print("Povolit");
			else cl.print("Zakazat");
		cl.print("</fveEnable>");

		cl.print("<fveEnable>");
			if (enableFveZimZahrada.enable == true)cl.print("Povolit");
			else cl.print("Zakazat");
		cl.print("</fveEnable>");
		                                
		//spinacky

		addr = &spin[0].t1Zap;// prvni prvek ve strukture
		for (size_t i = 0; i < 4; i++)
		{	
			cl.print("<sp0>");
			sprintf(cas, "%02u:%02u", *addr / 60, *addr % 60);
			cl.print(cas);
			addr ++;
			cl.print("</sp0>");
		}

		cl.print("<fveOutState>");
		if (spin[0].vystup_new == on)cl.print("sepnut");
		else cl.print("vypnut");
		cl.print("</fveOutState>");

		addr = &spin[1].t1Zap;// prvni prvek ve strukture
		for (size_t i = 0; i < 4; i++)
		{
			cl.print("<sp1>");
			sprintf(cas, "%02u:%02u", *addr / 60, *addr % 60);
			cl.print(cas);
			addr ++;
			cl.print("</sp1>");
		}
		cl.print("<fveOutState>");
		if (spin[1].vystup_new == on)cl.print("sepnut");
		else cl.print("vypnut");
		cl.print("</fveOutState>");

		addr = &spin[2].t1Zap;// prvni prvek ve strukture
		for (size_t i = 0; i < 4; i++)
		{
			cl.print("<sp2>");
			sprintf(cas, "%02u:%02u", *addr / 60, *addr % 60);
			cl.print(cas);
			addr ++;
			cl.print("</sp2>");
		}
		cl.print("<fveOutState>");
		if (spin[2].vystup_new == on)cl.print("sepnut");
		else cl.print("vypnut");
		cl.print("</fveOutState>");

		addr = &spin[3].t1Zap;// prvni prvek ve strukture
		for (size_t i = 0; i < 4; i++)
		{
			cl.print("<sp3>");
			sprintf(cas, "%02u:%02u", *addr / 60, *addr % 60);
			cl.print(cas);
			addr ++;
			cl.print("</sp3>");
		}

		cl.print("<fveOutState>");
		if (spin[3].vystup_new == on)cl.print("sepnut");
		else cl.print("vypnut");
		cl.print("</fveOutState>");

		addr = &spin[4].t1Zap;// prvni prvek ve strukture
		for (size_t i = 0; i < 4; i++)
		{
			cl.print("<sp4>");
			sprintf(cas, "%02u:%02u", *addr / 60, *addr % 60);
			cl.print(cas);
			addr ++;
			cl.print("</sp4>");
		}

		cl.print("<fveOutState>");
		if (spin[4].vystup_new == on)cl.print("sepnut");
		else cl.print("vypnut");
		cl.print("</fveOutState>");
	cl.print("</values>");
}

#pragma endregion
#pragma region OSTATNI
void Fve(uint16_t cas)
{

	//obyvak
	if (enableFveObyvak.enable == true)
	{
		if (enableFveObyvak.timeStart >= cas&&enableFveObyvak.timeStop <= cas)
		{
			enableFveObyvak.vystup_new = on;
			if (enableFveObyvak.vystup_new != enableFveObyvak.vystup_prew)
			{
				enableFveObyvak.vystup_prew = enableFveObyvak.vystup_new;
				digitalWrite(enableFveObyvak.outPin, enableFveObyvak.vystup_new);
			}
		}
		else
		{
			enableFveObyvak.vystup_new = off;
			if (enableFveObyvak.vystup_new != enableFveObyvak.vystup_prew)
			{
				enableFveObyvak.vystup_prew = enableFveObyvak.vystup_new;
				digitalWrite(enableFveObyvak.outPin, enableFveObyvak.vystup_new);
			}
		}
	}
	//zimni zahrada
	if (enableFveZimZahrada.enable == true)
	{
		if (enableFveZimZahrada.timeStart >= cas&&enableFveZimZahrada.timeStop <= cas)
		{
			enableFveZimZahrada.vystup_new = on;
			if (enableFveZimZahrada.vystup_new != enableFveZimZahrada.vystup_prew)
			{
				enableFveZimZahrada.vystup_prew = enableFveZimZahrada.vystup_new;
				digitalWrite(enableFveZimZahrada.outPin, enableFveZimZahrada.vystup_new);
			}
		}
		else
		{
			enableFveZimZahrada.vystup_new = off;
			if (enableFveZimZahrada.vystup_new != enableFveZimZahrada.vystup_prew)
			{
				enableFveZimZahrada.vystup_prew = enableFveZimZahrada.vystup_new;
				digitalWrite(enableFveZimZahrada.outPin, enableFveZimZahrada.vystup_new);
			}
		}
	}
}


#pragma endregion
