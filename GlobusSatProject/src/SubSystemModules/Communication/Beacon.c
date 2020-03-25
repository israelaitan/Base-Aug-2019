#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <satellite-subsystems/IsisTRXVU.h>

#include <hal/Timing/Time.h>
#include <hal/Storage/FRAM.h>

#include "SubSystemModules/Housekepping/TelemetryCollector.h"
#include "SubSystemModules/Communication/SatCommandHandler.h"

#include "GlobalStandards.h"
#include "FRAM_FlightParameters.h"

#include "SubSystemModules/Maintenance/Maintenance.h"
#include "SPL.h"
#include "Beacon.h"
#include "SatDataTx.h"

time_unix g_prev_beacon_time = 0;				// the time at which the previous beacon occured
time_unix g_beacon_interval_time = 0;
// marks the current beacon cycle(how many were transmitted before change in baud)
unsigned char g_current_beacon_period;
// every 'g_beacon_change_baud_period' beacon will be in 1200Bps and not 9600Bps
unsigned char g_beacon_change_baud_period;

void InitBeaconParams()
{
	//TODO: remove print after testing complete
	printf("Inside InitBeaconParams()");

	g_beacon_change_baud_period=DEFALUT_BEACON_BITRATE_CYCLE;
	FRAM_read((unsigned char*)&g_beacon_change_baud_period, BEACON_BITRATE_CYCLE_ADDR, BEACON_BITRATE_CYCLE_SIZE );
	g_beacon_interval_time= DEFAULT_BEACON_INTERVAL_TIME;
	FRAM_read((unsigned char*)&g_beacon_interval_time, BEACON_INTERVAL_TIME_ADDR, BEACON_INTERVAL_TIME_SIZE );

	//TODO: remove print after testing complete
	printf("InitBeaconParams() - setting bitrate to 9600");

	IsisTrxvu_tcSetAx25Bitrate(ISIS_TRXVU_I2C_BUS_INDEX, trxvu_bitrate_9600);


}

/*//Sets the bitrate to 1200 every third beacon and to 9600 otherwise
int BeaconSetBitrate()
{
	//TODO: remove print after testing complete
		print("Inside BeaconSetBitrate()");
	int err = 0;
	if (g_current_beacon_period % g_beacon_change_baud_period == 0) {
		err = IsisTrxvu_tcSetAx25Bitrate(ISIS_TRXVU_I2C_BUS_INDEX,
				trxvu_bitrate_1200);
		g_current_beacon_period = 0;
	}
	else {
		err = IsisTrxvu_tcSetAx25Bitrate(ISIS_TRXVU_I2C_BUS_INDEX,
				trxvu_bitrate_9600);
	}
	g_current_beacon_period++;
	return err;
}*/

//Beacon logic code
void BeaconLogic()
{
	//TODO: remove print after testing complete
	printf("Inside BeaconLogic()");
	sat_packet_t packet;
	WOD_Telemetry_t wod = { 0 };
	unsigned int id=  0xFFFFFFFF;
	if(CheckTransmitionAllowed())
	{
		if(CheckExecutionTime(g_prev_beacon_time, g_beacon_interval_time))
		{
			GetCurrentWODTelemetry(&wod);
			AssembleCommand((unsigned char *)&wod, sizeof(wod), trxvu_cmd_type, BEACON_SUBTYPE, id, 0, &packet);
			TransmitSplPacket(&packet, NULL);
			Time_getUnixEpoch((unsigned int *)&g_prev_beacon_time);
			//TODO: remove after testing complete
			printf("beacon sent - id: %d data: %s",packet.ID, packet.data );
		}
		else
		{
			//TODO: remove after testing complete
			printf("beacon time did not arrive");
		}

	}
	else
	{
		//TODO: remove after testing complete
		printf("beacon not allowed");
	}

}

int UpdateBeaconBaudCycle(unsigned char cycle)
{
	//TODO: remove print after testing complete
	printf("Inside UpdateBeaconBaudCycle() %c, %c" , cycle, DEFALUT_BEACON_BITRATE_CYCLE);

	if (cycle < DEFALUT_BEACON_BITRATE_CYCLE)
		return E_PARAM_OUTOFBOUNDS;
	int err = FRAM_write(&cycle, BEACON_BITRATE_CYCLE_ADDR, BEACON_BITRATE_CYCLE_SIZE );
	if (err!=0)
	{
		printf("cycle update to FRAM failed");
		return err;
	}
	else
	{
		//TODO: remove print after testing complete
		printf("cycle was updated to FRAM successfully");
	}

	g_beacon_change_baud_period = cycle;
	return E_NO_SS_ERR;
}


int UpdateBeaconInterval(time_unix intrvl)
{
	//TODO: remove print after testing complete
	printf("Inside UpdateBeaconInterval() interval: %ld, max: %d, min: %d" , intrvl, MAX_BEACON_INTERVAL, MIN_BEACON_INTERVAL);
	if(intrvl>MAX_BEACON_INTERVAL|| intrvl<MIN_BEACON_INTERVAL)
		return E_PARAM_OUTOFBOUNDS;

	int err = FRAM_write((unsigned char *)&intrvl, BEACON_INTERVAL_TIME_ADDR, BEACON_INTERVAL_TIME_SIZE );
	if (err!=0)
	{
		printf("interval update to FRAM failed");
		return err;
	}
	else
	{
		//TODO: remove print after testing complete
		printf("interval was updated to FRAM successfully");
	}

	g_beacon_interval_time = intrvl;
	return E_NO_SS_ERR;
}

/*
void TestBeacon()
{
	for(int i=0;i<NUMBEACON;i++)
	{
		Beaconlogic();
		vTaskDelay(AWATEDTIME);
	}
}
*/
