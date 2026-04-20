#include "LoRaWan_APP.h"
#include <Arduino.h>
#include "BLEDeviceScanner.h"

uint8_t devEui[] = {};  
bool overTheAirActivation = true;
uint8_t appEui[] = {};  // you should set whatever your TTN generates. TTN calls this the joinEUI, they are the same thing. 
uint8_t appKey[] = {};  // you should set whatever your TTN generates 

//These are only used for ABP, for OTAA, these values are generated on the Nwk Server, you should not have to change these values
uint8_t nwkSKey[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t appSKey[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint32_t devAddr =  (uint32_t)0x00000000;  

/*LoraWan channelsmask*/
uint16_t userChannelsMask[6]={ 0xFF00,0x0000,0x0000,0x0000,0x0000,0x0000 };

/*LoraWan region, select in arduino IDE tools*/
LoRaMacRegion_t loraWanRegion = ACTIVE_REGION;  // we define this as a user flag in the .ini file. 

/*LoraWan Class, Class A and Class C are supported*/
DeviceClass_t  loraWanClass = CLASS_A;

/*the application data transmission duty cycle.  value in [ms].*/
uint32_t appTxDutyCycle = 15000;

/*ADR enable*/
bool loraWanAdr = true;

// uint32_t license[4] = {};

/* Indicates if the node is sending confirmed or unconfirmed messages */
bool isTxConfirmed = true;

/* Application port */
uint8_t appPort = 1;
/*!
* Number of trials to transmit the frame, if the LoRaMAC layer did not
* receive an acknowledgment. The MAC performs a datarate adaptation,
* according to the LoRaWAN Specification V1.0.2, chapter 18.4, according
* to the following table:
*
* Transmission nb | Data Rate
* ----------------|-----------
* 1 (first)       | DR
* 2               | DR
* 3               | max(DR-1,0)
* 4               | max(DR-1,0)
* 5               | max(DR-2,0)
* 6               | max(DR-2,0)
* 7               | max(DR-3,0)
* 8               | max(DR-3,0)
*
* Note, that if NbTrials is set to 1 or 2, the MAC will not decrease
* the datarate, in case the LoRaMAC layer did not receive an acknowledgment
*/
uint8_t confirmedNbTrials = 8;

BLEDeviceScanner scanner;

/* Prepares the payload of the frame */
static void prepareTxFrame( uint8_t port )
{
	/*appData size is LORAWAN_APP_DATA_MAX_SIZE which is defined in "commissioning.h".
	*appDataSize max value is LORAWAN_APP_DATA_MAX_SIZE.
	*if enabled AT, don't modify LORAWAN_APP_DATA_MAX_SIZE, it may cause system hanging or failure.
	*if disabled AT, LORAWAN_APP_DATA_MAX_SIZE can be modified, the max value is reference to lorawan region and SF.
	*for example, if use REGION_CN470, 
	*the max value for different DR can be found in MaxPayloadOfDatarateCN470 refer to DataratesCN470 and BandwidthsCN470 in "RegionCN470.h".
	*/
    // This data can be changed, just make sure to change the datasize as well. 
	Summary results = scanner.getStats(SCAN_DURATION);
	unsigned short total = results.apples + results.microsofts + results.samsungs + results.others;
	Serial.printf("\nscanned for %d secs @ %d%% duty cycle, found %d likely personal devices:\n", SCAN_DURATION, SCAN_DUTY_CYCLE, total);
	Serial.printf("\tlaptops: %hu\n", results.laptops);
	Serial.printf("\tmobile devices: %hu\n", results.mobiles);
	Serial.printf("\twearables: %hu\n", results.wearables);
	Serial.printf("\tunknown: %hu\n", results.unknowns);

	Serial.println("suspected manufacturers:");
	Serial.printf("\tApple: %hu\n", results.apples);
	Serial.printf("\tMicrosoft: %hu\n", results.microsofts);
	Serial.printf("\tSamsung: %hu\n", results.samsungs);
	Serial.printf("\tGoogle: %hu\n", results.googles);
	Serial.printf("\tothers: %hu\n\n", results.others);

    appData[0] = 'M';
    appData[1] = (results.mobiles >> 8) & 0xFF;
    appData[2] = results.mobiles & 0xFF;

    appData[3] = 'L';
	appData[4] = (results.laptops >> 8) & 0xFF;
    appData[5] = results.laptops & 0xFF;

    appData[6] = 'W';
	appData[7] = (results.wearables >> 8) & 0xFF;
    appData[8] = results.wearables & 0xFF;

	appData[9] = 'U';
	appData[10] = (results.unknowns >> 8) & 0xFF;
    appData[11] = results.unknowns & 0xFF;

	appData[12] = 'A';
	appData[13] = 'A';
    appData[14] = 'P';
	appData[15] = 'L';
	appData[16] = (results.apples >> 8) & 0xFF;
    appData[17] = results.apples & 0xFF;

	appData[18] = 'G';
	appData[19] = 'O';
    appData[20] = 'O';
	appData[21] = 'G';
	appData[22] = (results.googles >> 8) & 0xFF;
    appData[23] = results.googles & 0xFF;

	appData[24] = 'M';
	appData[25] = 'S';
    appData[26] = 'F';
	appData[27] = 'T';
	appData[28] = (results.microsofts >> 8) & 0xFF;
    appData[29] = results.microsofts & 0xFF;

	appData[30] = 'S';
	appData[31] = 'M';
    appData[32] = 'S';
	appData[33] = 'G';
	appData[34] = (results.samsungs >> 8) & 0xFF;
    appData[35] = results.samsungs & 0xFF;

	appData[36] = 'O';
	appData[37] = 'T';
    appData[38] = 'H';
	appData[39] = 'E';
	appData[40] = 'R';
	appData[41] = (results.others >> 8) & 0xFF;
    appData[42] = results.others & 0xFF;

	// edit location name here
	appData[43] = 'R';
	appData[44] = 'I';
	appData[45] = 'C';
	appData[46] = 'E';

	appDataSize = 47;
}

RTC_DATA_ATTR bool firstrun = true;

void setup() 
{

	Serial.begin(115200);
	Mcu.begin();

  	if (firstrun) 
	{
    	LoRaWAN.displayMcuInit();
    	firstrun = false;
  	}

	deviceState = DEVICE_STATE_INIT;
	scanner.begin();
}

void loop()
{
	switch( deviceState )
	{
		case DEVICE_STATE_INIT:
		{
#if(LORAWAN_DEVEUI_AUTO)
			LoRaWAN.generateDeveuiByChipID();
#endif
			LoRaWAN.init(loraWanClass,loraWanRegion);
			break;
		}
		case DEVICE_STATE_JOIN:
		{
      		LoRaWAN.displayJoining();
			LoRaWAN.join();
			
			if (deviceState == DEVICE_STATE_SEND)
			 	LoRaWAN.displayJoined();
			break;
		}
		case DEVICE_STATE_SEND:
		{
      		LoRaWAN.displaySending();
			prepareTxFrame( appPort );
			LoRaWAN.send();
			deviceState = DEVICE_STATE_CYCLE;
			break;
		}
		case DEVICE_STATE_CYCLE:
		{
			// Schedule next packet transmission
			txDutyCycleTime = appTxDutyCycle + randr( 0, APP_TX_DUTYCYCLE_RND );
			LoRaWAN.cycle(txDutyCycleTime);
			deviceState = DEVICE_STATE_SLEEP;
			break;
		}
		case DEVICE_STATE_SLEEP:
		{
      		LoRaWAN.displayAck();
			LoRaWAN.sleep(loraWanClass);
			break;
		}
		default:
		{
			deviceState = DEVICE_STATE_INIT;
			break;
		}
	}
}
