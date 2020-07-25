#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <hal/Storage/FRAM.h>
#include <hal/Timing/Time.h>
#include <hal/errors.h>

#include <satellite-subsystems/IsisTRXVU.h>
#include "SubSystemModules/PowerManagment/EPSOperationModes.h"
#include "SubSystemModules/Maintenance/Log.h"
#include "GlobalStandards.h"
#include "SatDataTx.h"

Boolean 		g_mute_flag = MUTE_OFF;				// mute flag - is the mute enabled
time_unix 		g_mute_end_time = 0;				// time at which the mute will end

xSemaphoreHandle xIsTransmitting = NULL; // mutex on transmission.

void InitTxModule()
{
	if(NULL == xIsTransmitting)
		vSemaphoreCreateBinary(xIsTransmitting);
}

int muteTRXVU(time_unix duration) {
	if (duration > MAX_MUTE_TIME) {
		return -2;
	}
	time_unix curr_tick_time = 0;
	Time_getUnixEpoch((unsigned int *)&curr_tick_time);

	g_mute_end_time = curr_tick_time + duration;
	g_mute_flag = MUTE_ON;
	logg(TRXInfo, "I:************************inside muteTRXVU: %lu\n", (long unsigned int)g_mute_end_time);
	return 0;
}


void UnMuteTRXVU() {
	g_mute_end_time = 0;
	g_mute_flag = MUTE_OFF;
	logg(TRXInfo, "I:*********************Setting Mute to OFF\n");
}

Boolean GetMuteFlag() {
	return g_mute_flag;
}

Boolean CheckForMuteEnd() {
	time_unix curr_tick_time = 0;
	Time_getUnixEpoch((unsigned int *)&curr_tick_time);
	if (curr_tick_time > g_mute_end_time)
	{
		logg(TRXInfo, "I:Mute End time reached\n");
		return TRUE;
	}
	else
	{
		logg(TRXInfo, "I:Mute End time NOT reached\n");
		return FALSE;
	}

}

int GetNumberOfFramesInBuffer() {
	unsigned short frameCounter = 0;
	int err = IsisTrxvu_rcGetFrameCount(0, &frameCounter);
	if (0 != err) {
		return -1;
	}
	return frameCounter;
}

Boolean IsTransmitting() {
	if(pdTRUE == xSemaphoreTake(xIsTransmitting,0)){
		xSemaphoreGive(xIsTransmitting);
		return FALSE;
	}
	return TRUE;
}

Boolean CheckTransmitionAllowed() {
	Boolean low_voltage_flag = TRUE;

	low_voltage_flag = EpsGetLowVoltageFlag();

	if (g_mute_flag == MUTE_OFF && low_voltage_flag == FALSE)
	{
		if(pdTRUE == xSemaphoreTake(xIsTransmitting,0))
		{
			xSemaphoreGive(xIsTransmitting);
			logg(TRXInfo, "I:TRASMITION ALLOWED\n");
			return TRUE;
		}
	}
	logg(TRXInfo, "I:TRASMITION NOT ALLOWED\n");
	return FALSE;
}

int GetTrxvuBitrate(ISIStrxvuBitrateStatus *bitrate) {
	if (NULL == bitrate) {
		return E_NOT_INITIALIZED;
	}
	ISIStrxvuTransmitterState trxvu_state;
	int err = IsisTrxvu_tcGetState(ISIS_TRXVU_I2C_BUS_INDEX, &trxvu_state);

	if (E_NO_SS_ERR == err) {
		*bitrate = trxvu_state.fields.transmitter_bitrate;
	}
	return err;

}

int TransmitDataAsSPL_Packet(sat_packet_t *cmd, unsigned char *data, unsigned int length) {
	int err = 0;
	sat_packet_t packet = { 0 };
	if (NULL != cmd) {
		err = AssembleCommand(data, length, cmd->cmd_type, cmd->cmd_subtype,
				cmd->ID, cmd->ordinal, cmd->targetSat, &packet);
	} else {
		err = AssembleCommand(data, length, 0xFF, 0xFF, 0xFFFF, 0, cmd->targetSat, &packet);
	}
	if (err != 0) {
		return err;
	}
	err = TransmitSplPacket(&packet, NULL);
	return err;
}

int TransmitSplPacket(sat_packet_t *packet, int *avalFrames) {
	if (!CheckTransmitionAllowed()) {
		return -1;
	}

	if (NULL == packet) {
		return E_NOT_INITIALIZED;
	}

	int err = 0;
	unsigned int data_length = packet->length + SAT_PACKET_HEADER_LENGTH;

	if (xSemaphoreTake(xIsTransmitting,SECONDS_TO_TICKS(1)) != pdTRUE) {
		return E_GET_SEMAPHORE_FAILED;
	}
	err = IsisTrxvu_tcSendAX25DefClSign(ISIS_TRXVU_I2C_BUS_INDEX,
			(unsigned char*) packet, data_length, (unsigned char*) avalFrames);

	xSemaphoreGive(xIsTransmitting);

	return err;
}

