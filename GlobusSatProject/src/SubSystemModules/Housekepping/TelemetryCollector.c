#include <hcc/api_fat.h>
#include "GlobalStandards.h"
#include <satellite-subsystems/isis_eps_driver.h>

#include <satellite-subsystems/IsisTRXVU.h>
#include <satellite-subsystems/IsisAntS.h>
#include <satellite-subsystems/IsisSolarPanelv2.h>
#include <hal/Timing/Time.h>

#include <string.h>

#include "TelemetryCollector.h"
#include "TelemetryFiles.h"
#include "TLM_management.h"
#include "SubSystemModules/Maintenance/Maintenance.h"
#include "SubSystemModules/Maintenance/Log.h"
#include "SubSystemModules/Communication/SatDataTx.h"

time_unix tlm_save_periods[NUM_OF_SUBSYSTEMS_SAVE_FUNCTIONS] = {0};
time_unix tlm_last_save_time[NUM_OF_SUBSYSTEMS_SAVE_FUNCTIONS]= {0};


int InitTelemetryCollector() {
	return FRAM_read((unsigned char*)tlm_save_periods,TLM_SAVE_PERIOD_START_ADDR,NUM_OF_SUBSYSTEMS_SAVE_FUNCTIONS*sizeof(time_unix));
}

int GetTelemetryFilenameByType(tlm_type tlm_type, char filename[MAX_F_FILE_NAME_SIZE])
{
	if(NULL == filename){
		return -1;
	}
	switch (tlm_type) {
	case tlm_wod:
		strcpy(filename,FILENAME_WOD_TLM);
		break;
	case tlm_eps_raw_mb:
		strcpy(filename,FILENAME_EPS_RAW_MB_TLM);
		break;
	case tlm_eps_eng_mb:
		strcpy(filename,FILENAME_EPS_ENG_MB_TLM);
		break;
	case tlm_solar:
		strcpy(filename,FILENAME_SOLAR_PANELS_TLM);
		break;
	case tlm_tx:
		strcpy(filename,FILENAME_TX_TLM);
		break;
	case tlm_rx:
		strcpy(filename,FILENAME_RX_TLM);
		break;
	case tlm_antenna:
		strcpy(filename,FILENAME_ANTENNA_TLM);
		break;
	case tlm_log:
		strcpy(filename,FILENAME_LOG_TLM);
		break;
	case tlm_log_bckp:
		strcpy(filename,FILENAME_LOG_BCKP_TLM);
		break;
	default:
		return -2;
	}
	return 0;
}


void TelemetryCollectorLogic()
{
	if (CheckExecutionTime(tlm_last_save_time[eps_tlm],tlm_save_periods[eps_tlm])){
		TelemetrySaveEPS();
		Time_getUnixEpoch((unsigned int *)(&tlm_last_save_time[eps_tlm]));
		logg(info, "I:TelemetrySaveEPS, time: %lu\n", tlm_last_save_time[eps_tlm]);
	}

	if (CheckExecutionTime(tlm_last_save_time[trxvu_tlm],tlm_save_periods[trxvu_tlm])){
		TelemetrySaveTRXVU();
		logg(info, "I:TelemetrySaveTRXVU\n");
		Time_getUnixEpoch((unsigned int *)(&tlm_last_save_time[trxvu_tlm]));
	}

	if (CheckExecutionTime(tlm_last_save_time[ant_tlm],tlm_save_periods[ant_tlm])){
		TelemetrySaveANT();
		logg(info, "I:TelemetrySaveANT\n");
		Time_getUnixEpoch((unsigned int *)(&tlm_last_save_time[ant_tlm]));
	}

	if (CheckExecutionTime(tlm_last_save_time[solar_panel_tlm],tlm_save_periods[solar_panel_tlm])){
		TelemetrySaveSolarPanels();
		logg(info, "I:TelemetrySaveSolarPanels\n");
		Time_getUnixEpoch((unsigned int *)(&tlm_last_save_time[solar_panel_tlm]));
	}

	if (CheckExecutionTime(tlm_last_save_time[wod_tlm],tlm_save_periods[wod_tlm])){
		TelemetrySaveWOD();
		logg(info, "I:TelemetrySaveWOD\n");
		Time_getUnixEpoch((unsigned int *)(&tlm_last_save_time[wod_tlm]));
	}

}

#define SAVE_FLAG_IF_FILE_CREATED(type)	if(FS_SUCCSESS != res &&NULL != tlms_created){tlms_created[(type)] = FALSE_8BIT;}

void TelemetryCreateFiles(Boolean8bit tlms_created[NUMBER_OF_TELEMETRIES])
{
	FileSystemResult res;

	// -- EPS files
	res = c_fileCreate(FILENAME_EPS_RAW_MB_TLM,sizeof(isis_eps__gethousekeepingraw__from_t));
	SAVE_FLAG_IF_FILE_CREATED(tlm_eps_raw_mb)

	res = c_fileCreate(FILENAME_EPS_ENG_MB_TLM,sizeof(isis_eps__gethousekeepingeng__from_t));
	SAVE_FLAG_IF_FILE_CREATED(tlm_eps_eng_mb);

	// -- TRXVU files
	res = c_fileCreate(FILENAME_TX_TLM,sizeof(ISIStrxvuTxTelemetry));
	SAVE_FLAG_IF_FILE_CREATED(tlm_tx);

	res = c_fileCreate(FILENAME_RX_TLM,sizeof(ISIStrxvuRxTelemetry));
	SAVE_FLAG_IF_FILE_CREATED(tlm_eps_raw_mb);

	// -- ANT files
	res = c_fileCreate(FILENAME_ANTENNA_TLM,sizeof(ISISantsTelemetry));
	SAVE_FLAG_IF_FILE_CREATED(tlm_antenna);

	//-- SOLAR PANEL files
	res = c_fileCreate(FILENAME_SOLAR_PANELS_TLM,sizeof(int32_t)*ISIS_SOLAR_PANEL_COUNT);
	SAVE_FLAG_IF_FILE_CREATED(tlm_solar);

	//-- LOG files
	res = c_fileCreate(FILENAME_LOG_TLM, LOG_BUFFER_SIZE);
	SAVE_FLAG_IF_FILE_CREATED(tlm_log);
	res = c_fileCreate(FILENAME_LOG_BCKP_TLM, LOG_BUFFER_SIZE);
	SAVE_FLAG_IF_FILE_CREATED(tlm_log_bckp);
}

void TelemetrySaveEPS()
{
	int err = 0;

	isis_eps__gethousekeepingraw__from_t tlm_mb_raw;
	err = isis_eps__gethousekeepingraw__tm(EPS_I2C_BUS_INDEX, &tlm_mb_raw);
	if (err == 0)
		c_fileWrite(FILENAME_EPS_RAW_MB_TLM, &tlm_mb_raw);
	else
		logg(error, "E=%d isis_eps__gethousekeepingraw__tm\n", err);

	isis_eps__gethousekeepingeng__from_t tlm_mb_eng;
	err = isis_eps__gethousekeepingeng__tm(EPS_I2C_BUS_INDEX, &tlm_mb_eng);

	if (err == 0)
		c_fileWrite(FILENAME_EPS_ENG_MB_TLM, &tlm_mb_eng);
	else
		logg(error, "E=%d isis_eps__gethousekeepingeng__tm\n", err);

}

void TelemetrySaveTRXVU()
{
	int err = 0;
	ISIStrxvuTxTelemetry tx_tlm;
	err = IsisTrxvu_tcGetTelemetryAll(ISIS_TRXVU_I2C_BUS_INDEX, &tx_tlm);
	if (err == 0)
	{
		c_fileWrite(FILENAME_TX_TLM, &tx_tlm);
	}

	ISIStrxvuRxTelemetry rx_tlm;
	err = IsisTrxvu_rcGetTelemetryAll(ISIS_TRXVU_I2C_BUS_INDEX, &rx_tlm);
	if (err == 0)
	{
		c_fileWrite(FILENAME_RX_TLM, &rx_tlm);
	}

}

void TelemetrySaveANT()
{
	int err = 0;
	ISISantsTelemetry ant_tlmA, ant_tlmB;
	err = IsisAntS_getAlltelemetry(ISIS_TRXVU_I2C_BUS_INDEX, isisants_sideA,
			&ant_tlmA);
	if (err == 0)
	{
		c_fileWrite(FILENAME_ANTENNA_TLM, &ant_tlmA);
	}
	err = IsisAntS_getAlltelemetry(ISIS_TRXVU_I2C_BUS_INDEX, isisants_sideB,
			&ant_tlmB);
	if (err == 0)
	{
		c_fileWrite(FILENAME_ANTENNA_TLM, &ant_tlmB);
	}

}

int getSolarPanelsTLM(int32_t *t)
{
	int err = 0;
	uint8_t fault;

	err =  IsisSolarPanelv2_getTemperature(ISIS_SOLAR_PANEL_0, &t[0], &fault);
	err += IsisSolarPanelv2_getTemperature(ISIS_SOLAR_PANEL_1, &t[1], &fault);
	err += IsisSolarPanelv2_getTemperature(ISIS_SOLAR_PANEL_2, &t[2], &fault);
	err += IsisSolarPanelv2_getTemperature(ISIS_SOLAR_PANEL_3, &t[3], &fault);
	err += IsisSolarPanelv2_getTemperature(ISIS_SOLAR_PANEL_4, &t[4], &fault);
	err += IsisSolarPanelv2_getTemperature(ISIS_SOLAR_PANEL_5, &t[5], &fault);
	err += IsisSolarPanelv2_getTemperature(ISIS_SOLAR_PANEL_6, &t[6], &fault);
	err += IsisSolarPanelv2_getTemperature(ISIS_SOLAR_PANEL_7, &t[7], &fault);
	err += IsisSolarPanelv2_getTemperature(ISIS_SOLAR_PANEL_8, &t[8], &fault);

	return err;
}

void TelemetrySaveSolarPanels()
{
	int err = 0;
	int32_t t[ISIS_SOLAR_PANEL_COUNT];

	if (IsisSolarPanelv2_getState() == ISIS_SOLAR_PANEL_STATE_AWAKE)
	{
		err = getSolarPanelsTLM(t);
		if (err == ISIS_SOLAR_PANEL_STATE_AWAKE * ISIS_SOLAR_PANEL_COUNT)
		{
			c_fileWrite(FILENAME_SOLAR_PANELS_TLM, t);
		}
	}
}

void TelemetrySaveWOD()
{
	WOD_Telemetry_t wod = { 0 };
	GetCurrentWODTelemetry(&wod);
	c_fileWrite(FILENAME_WOD_TLM, &wod);
}

void GetCurrentWODTelemetry(WOD_Telemetry_t *wod)
{
	if (NULL == wod){
		return;
	}

	memset(wod,0,sizeof(*wod));
	int err = 0;

	F_SPACE space = { 0 };
	int drivenum = f_getdrive();
	err = f_getlasterror();
	//TODO:check drivenum=-1 error = 37?
	err = f_getfreespace(drivenum, &space);
	if (err == F_NO_ERROR){
		wod->free_memory = space.free;
		wod->corrupt_bytes = space.bad;
	}
	time_unix current_time = 0;
	Time_getUnixEpoch((unsigned int *)&current_time);
	wod->sat_time = current_time;

	isis_eps__gethousekeepingrawincdb__from_t hk_tlm;
	isis_eps__gethousekeepingeng__from_t hk_tlm_cdb;

	err = isis_eps__gethousekeepingrawincdb__tm( EPS_I2C_BUS_INDEX, &hk_tlm );
	err += isis_eps__gethousekeepingeng__tm( EPS_I2C_BUS_INDEX, &hk_tlm_cdb );

	if(err == 0){
		//TODO: map correct values
		wod->vbat = hk_tlm_cdb.fields.dist_input.fields.volt;
		wod->current_3V3 = hk_tlm_cdb.fields.batt_input.fields.volt;
		wod->current_5V = hk_tlm_cdb.fields.batt_input.fields.volt;
		wod->volt_3V3 = hk_tlm_cdb.fields.volt_vd2;
		wod->volt_5V = hk_tlm_cdb.fields.volt_vd1;
		wod->charging_power = hk_tlm_cdb.fields.batt_input.fields.power;
		wod->consumed_power = hk_tlm_cdb.fields.dist_input.fields.power;
	}
    //Get number of resets is not managed
	FRAM_read((unsigned char*)&wod->number_of_resets, NUMBER_OF_RESETS_ADDR, NUMBER_OF_RESETS_SIZE);
}

//get EPS TLM
int CMD_getEPS_TLM(sat_packet_t *cmd)
{
	int err1, err2;
	int size = sizeof(unsigned char)*116;

	unsigned char rawData[232];
	isis_eps__gethousekeepingraw__from_t tlm_mb_raw;
	err1 = isis_eps__gethousekeepingraw__tm(EPS_I2C_BUS_INDEX, &tlm_mb_raw);

	if (err1 == 0)	{
		memcpy(&rawData, (unsigned char*)&tlm_mb_raw.raw, size);
	} else 	{
		logg(error, "E=%d isis_eps__gethousekeepingraw__tm\n", err1);
	}


	isis_eps__gethousekeepingeng__from_t tlm_mb_eng;
	err2 = isis_eps__gethousekeepingeng__tm(EPS_I2C_BUS_INDEX, &tlm_mb_eng);

	if (err2 == 0)	{
		memcpy(&rawData + size, (unsigned char*)&tlm_mb_eng.raw, size);
	} else {
		logg(error, "E=%d isis_eps__gethousekeepingeng__tm\n", err2);
	}

	if(err1 == 0 || err2 == 0)
	{
		TransmitDataAsSPL_Packet(cmd, (unsigned char*)rawData, size*2);
		return 0;
	}


	return err1;
}

// Get Solar Panels TLM
int CMD_getSolar_TLM(sat_packet_t *cmd)
{
	int err = 0;
	int32_t t[ISIS_SOLAR_PANEL_COUNT];

	if (IsisSolarPanelv2_getState() == ISIS_SOLAR_PANEL_STATE_AWAKE)
	{
		err = getSolarPanelsTLM(t);
		if (err == ISIS_SOLAR_PANEL_STATE_AWAKE * ISIS_SOLAR_PANEL_COUNT)
		{
			TransmitDataAsSPL_Packet(cmd, (unsigned char*) t, sizeof(int32_t)*ISIS_SOLAR_PANEL_COUNT);
		}
	}

	return err;
}

// Get TRXVU TLM
int CMD_getTRXVU_TLM(sat_packet_t *cmd)
{
	int err1, err2;
	unsigned char rawData[TRXVU_ALL_RXTELEMETRY_SIZE*2];
	int size = TRXVU_ALL_RXTELEMETRY_SIZE * sizeof(unsigned char);

	ISIStrxvuTxTelemetry tx_tlm;
	err1 = IsisTrxvu_tcGetTelemetryAll(ISIS_TRXVU_I2C_BUS_INDEX, &tx_tlm);
	if (err1 == 0)
	{
		memcpy(&rawData, (unsigned char*)&tx_tlm.raw, size);
	}

	ISIStrxvuRxTelemetry rx_tlm;
	err2 = IsisTrxvu_rcGetTelemetryAll(ISIS_TRXVU_I2C_BUS_INDEX, &rx_tlm);
	if (err2 == 0)
	{
		memcpy(&rawData+size, (unsigned char*)&rx_tlm.raw, size);
	}

	if(err1 == 0 || err2 == 0)
	{
		TransmitDataAsSPL_Packet(cmd, (unsigned char*)rawData, size*2);
		return 0;
	}

	return err1;
}

// Get Antennas TLM
int CMD_getAnts_TLM(sat_packet_t *cmd)
{
	int err = 0;
	ISISantsTelemetry ant_tlm;
	err = IsisAntS_getAlltelemetry(ISIS_TRXVU_I2C_BUS_INDEX, isisants_sideA, &ant_tlm);
	if (err != 0)
	{
		err = IsisAntS_getAlltelemetry(ISIS_TRXVU_I2C_BUS_INDEX, isisants_sideB, &ant_tlm);
	}

	//success get telemetry
	if (err == 0)
	{
		//send data
		TransmitDataAsSPL_Packet(cmd, (unsigned char*) &ant_tlm,sizeof(ant_tlm));
	}

	return err;
}

