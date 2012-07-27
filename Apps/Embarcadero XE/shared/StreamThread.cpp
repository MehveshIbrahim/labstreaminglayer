//---------------------------------------------------------------------------


#pragma hdrstop

#include "StreamThread.h"
#include "stdio.h"

//---------------------------------------------------------------------------

#pragma package(smart_init)

#define MAX_CHANNELS 128

/*
 * Use this constructor when reading data manually.
 * Primary used to have asynchronous opening of inlet.
 */


TStreamThread::TStreamThread(char *typeNam):TThread(false)
{
	FreeOnTerminate=false;
	strcpy(typeName,typeNam);
	ProcessDataI32 = NULL;
	ProcessDataF32 = NULL;
	ProcessDataF64 = NULL;
	ProcessDataString = NULL;

	connected = false;
}

TStreamThread::TStreamThread(char * typeNam, TProcessDataI32 PD):TThread(false)
{
		FreeOnTerminate=false;
		strcpy(typeName,typeNam);
		ProcessDataI32 = PD;
		ProcessDataF32 = NULL;
		ProcessDataF64 = NULL;
		ProcessDataString = NULL;
		connected = false;

		//if(errcode != 0)
		//	THROW ERROR

};

TStreamThread::TStreamThread(char * typeNam, TProcessDataF32 PD):TThread(false)
{
		FreeOnTerminate=false;
		strcpy(typeName,typeNam);
		ProcessDataI32 = NULL;
		ProcessDataF32 = PD;
		ProcessDataF64 = NULL;
		ProcessDataString = NULL;
		connected = false;

		//if(errcode != 0)
		//	THROW ERROR

};

TStreamThread::TStreamThread(char * typeNam, TProcessDataF64 PD):TThread(false)
{
		FreeOnTerminate=false;
		strcpy(typeName,typeNam);
		ProcessDataI32 = NULL;
		ProcessDataF32 = NULL;
		ProcessDataF64 = PD;
		ProcessDataString = NULL;

		connected = false;

};

TStreamThread::TStreamThread(char * typeNam, TProcessDataString PD):TThread(false)
{
		FreeOnTerminate=false;
		strcpy(typeName,typeNam);
		ProcessDataI32 = NULL;
		ProcessDataF32 = NULL;
		ProcessDataF64 = NULL;
		ProcessDataString = PD;

		connected = false;

};

__fastcall TStreamThread::~TStreamThread()
{
	lsl_destroy_inlet(inlet);
};



void __fastcall TStreamThread::Execute()
{



	while (!Terminated)
	{
		if(!connected) {
			lsl_streaminfo info;

			int streamsFound = lsl_resolve_bypred(&info,1, typeName, 1,1.0);

			if(streamsFound < 1) {
				printf("Requested Stream not resolved by timeout.\n");
				Sleep(5);
				continue;
			}


			inlet = lsl_create_inlet(info, 300, LSL_NO_PREFERENCE,1);
			lsl_open_stream(inlet,1.0,&errcode);
			//fills out desc field in xml header
			info = lsl_get_fullinfo(inlet,1.0,&errcode);

			if(errcode) {
				printf("error opening stream: %d\n", errcode);
				Sleep(5);

				continue;
			} else {
				connected = true;
				nChannels = lsl_get_channel_count(info);
				samplingRate = lsl_get_nominal_srate(info);
				xmlHeader = lsl_get_xml(info);
				printf("%s\n", xmlHeader);
				if(nChannels > MAX_CHANNELS) {
					printf("Too many channels\n");
					Sleep(5);
					continue;
					//THROW ERROR or replace with new1d
				}
			}
		}

		if(ProcessDataI32) {
			int buf[MAX_CHANNELS];
			double timestamp = lsl_pull_sample_i(inlet,buf, nChannels, 1.0, &errcode);

			if(timestamp) ProcessDataI32(buf, nChannels, samplingRate);
			else {
				Sleep(5);
				continue;
			}
		}

		if(ProcessDataF32) {
			float buf[MAX_CHANNELS];
			double timestamp = lsl_pull_sample_f(inlet,buf, nChannels, 1.0, &errcode);

			if(timestamp) ProcessDataF32(buf, nChannels, samplingRate);
			else {
				Sleep(5);
				continue;
			}
		}

		else if(ProcessDataF64) {
			double buf[MAX_CHANNELS];
			double timestamp = lsl_pull_sample_d(inlet,buf, nChannels, 1.0, &errcode);
			if(timestamp) ProcessDataF64(buf, nChannels,samplingRate);
			else {
				Sleep(5);
				continue;
			}
		} else if(ProcessDataString) {
			char *buf = NULL;
			double timestamp = lsl_pull_sample_str(inlet, &buf, 1, 1.0, &errcode);
			if(timestamp) ProcessDataString(buf, nChannels, samplingRate);
		} else {
			Sleep(5);
		}
	}
}