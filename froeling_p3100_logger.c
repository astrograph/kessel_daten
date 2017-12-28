/*--------------------------------------------------------------
 *	$Logfile: froeling_test.c $
 *	$Author: MKOT $
 *	$Revision: 1 $
 *	$Date: 29.12.12 13:03 $
 *--------------------------------------------------------------
 *	$Log: floeling_test.c $
 *
*--------------------------------------------------------------
 *
 *	Developer:	Markus Kotschenreuther, 2012
 *	PURPOSE:	Test communication with P3100 heater from Froeling
 *	PROCESS: 	-
 *	FUNCTIONS:	main(..)
 *--------------------------------------------------------------*/
/************************* compiler switches ********************************/
//#define WINDOWS
#define LINUX

/************************* includes *****************************************/
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#ifdef WINDOWS
	#include <windows.h>
	#include <conio.h>
	#include <sys\timeb.h>
#endif

#ifdef LINUX
	#include <termios.h>
	#include <unistd.h>
	#include <sys/stat.h>
	#include <fcntl.h>
	#include <time.h>
#endif

/************************* defines ******************************************/
#define ESC 27	// escape

#define TRUE  1
#define FALSE 0

#define RECEIVE_BUF_SIZE 512

#define LOGFILEPATH   "./comlog"
#define LOGFILESIZE   500000
#define ACTUALDATA    "./current_data.txt"

/************************* macros *******************************************/
/************************* typedefs *****************************************/
// log message identifier
typedef enum {
	LOG_SEND,
	LOG_RECEIVE,
	LOG_ERR,
	LOG_INFO
}T_LOG_IDENT;

typedef enum {
	STM_INIT,
	STM_RECEIVE_MSG,
	STM_HANDLE_MA,
	STM_HANDLE_M1,
	STM_HANDLE_M2,
	STM_HANDLE_MI
}T_STM_RECEIVE;

typedef enum {
	STM_SUB_RECEIVE_HEADER,
	STM_SUB_RECEIVE_REPLY_BODY,
	STM_SUB_RECEIVE_MESSAGE_BODY,
}T_STM_RECEIVE_SUB;

typedef enum {
	FROE_PARAM_WORKMODE = 155,
}T_FROE_PARAMID;

typedef enum {
	FROE_WORKMODE_WINTER = 0,
	FROE_WORKMODE_SOMMER = 1,
	FROE_WORKMODE_UEBERGANG = 2,
	FROE_WORKMODE_SCHEITHOLZ = 3,
	FROE_WORKMODE_REINIGEN = 4,
}T_FROE_WORKMODE;

/************************* global variables *********************************/
#ifdef WINDOWS
	HANDLE hCom;
#endif

#ifdef LINUX
	int hCom;
#endif

int DEBUG=0;
FILE *DataFile;
FILE *OutputFile;

int DataFileCounter=0; // Zaehler, um nur jede DataFileDelay-te Sekunde einen Eintrag in DataFile vorzunehmen
int DataFileDelay=1;

int OutputFileCounter=0; // Zaehler, um nur jede OutputFileDelay-te Sekunde einen Eintrag in OutputFile vorzunehmen
int OutputFileDelay=1;

int Output2Stdout=0; // Datenausgabe auch nach stdout (1) oder nicht (0)

short glsUserID;
int ValueCount = 0;
char *ValueNames[100];

char DataFileName[37]; // fuer den Namen des Datalogs
struct tm *tmnow;     // fuer die Bestimmung der aktuellen Uhrzeit
time_t tnow;           // fuer die Bestimmung der aktuellen Uhrzeit


/************************* forward declarations *****************************/
void PrintComLog(T_LOG_IDENT logident, char* buffer, int bufferlength);

int  OpenComPort(int PortId, int BaudRate);
int  ComSend(char* SendBuffer, int bytes2send);
int  ComReceive(char* ReceiveBuffer, int bytes2receive);

void Send_Msg(char* msg);

void Set_Time(void);
void Set_ParamValue(T_FROE_PARAMID ParamID, int value);

void CreateCommand(char* cmd, char* Param, int ParamLen, char* sendbuffer);
void CreateCheckSum(char* buf, int buffsize, char* checksum);
int  checkCheckSum(char* buf, int buffsize);
void sendAcq(char* buf, int buffsize);
void sendNAcq(char* buf, int buffsize);

void GotoXY(int x, int y);
int convert_FroelingTime(char x);
char convert_2_FroelingTime(int x);
void PrepareScreen(void);
void LinesUp(int nrows);
void SetDataFileName(void);

/************************* program ******************************************/

/*****************************************************************************
 ----------------------------------------
      MAIN FUNCTION
 ----------------------------------------
 ****************************************************************************/
int main (int argc, char *argv[])
{
	// local variables

	int iPortid = 0;

	if (argc<5) {
		printf("usage: %s <ComPortNr. 1-6> <DataFileInterval in sec> <OutpurFileInterval in sec> <0,1 Output nach stdout>\n",argv[0]);
		exit (0);
	}
	else{	
		iPortid=atoi(argv[1]);
		DataFileDelay = atoi(argv[2]);
		OutputFileDelay = atoi(argv[3]);
		Output2Stdout = atoi(argv[4]);
	}

	if (Output2Stdout) printf("Datenausgabe nach stdout aktivert!\n");
	else printf("Datenausgabe nach stdout deaktivert!\n");
	
	if (argc>5) DEBUG=1;
	if (DEBUG>0) printf("Debug aktiviert!\n");
	
	SetDataFileName();
	printf("Log data every %d seconds to file %s\n",DataFileDelay,DataFileName);
	char cReceiveBuffer[RECEIVE_BUF_SIZE];
	char cKb = '\0';
	int iInBuffPointer = 0;
	char cRbSendFlag = 0;
	unsigned int  iMsgLength = 0;
	int k = 0;
	int i = 0;
	char cTime[50];
	
	T_STM_RECEIVE tCurrentMainState = STM_INIT;
	T_STM_RECEIVE_SUB tCurrentSubState = STM_SUB_RECEIVE_HEADER;

	// Message at:
	// Byte:	 1| 2| 3| 4| 5| 6  ...
	// --------------------------------------------
	// HEX:		4d|32|07|57|50|20|25|01|03|12|01|88
	// --------------------------------------------
	// ASCII:	 M| 2| ...
	// --------------------------------------------
	// Content: Ident| l|   Datablock        | CHS |
	// Blocks : Header  |   Body             | CHS |
	//
	// Byte 1,2:	Message identifier (M1 = measurement values / M2 = time)
	// Byte 3		Message data block length in bytes (l)
	// Byte 4...	Datablock
	// Last 2 Bytes:Checksum (CHS)
	/*
	04< MA.I...+Zustand..                     4D 41 0C 49 00 1C 07 2B 5A 75 73 74 61 6E 64 04 1A    OK
	                                          M  A  12                Z  u  s  t  a  n  d | CHS

	06< MA.I....ROST.h                        4D 41 09 49 00 1D 07 1C 52 4F 53 54 02 68    OK
	                                          M  A  09                R  O  S  T | CHS

	08< MA.I....Kesseltemp..                  4D 41 0F 49 00 00 00 03 4B 65 73 73 65 6C 74 65 6D 70 05 06    OK
	                                          M  A  15                K  e  s  s  e  l             | CHS

	10< MA.I....Abgastemp..�                  4D 41 0F 49 00 05 01 04 41 62 67 61 73 74 65 6D 70 2E 04 B2    OK
	                                          M  A  15                A  b  g  a  s  t  e          | CHS

	12< MA.I....Abgas..SW..�                  4D 41 0F 49 00 06 05 0D 41 62 67 61 73 2E 20 53 57 20 03 F4    OK
	                                          M  A  15                A  b  g  a  s

	14< MA.I....KessStellGr.]                 4D 41 10 49 00 19 05 05 4B 65 73 73 53 74 65 6C 6C 47 72 05 5D    OK
	                                          M  A  16                K  e  s  s

	16< MA.I...*Saugzug...G                   4D 41 0E 49 00 0F 03 2A 53 61 75 67 7A 75 67 20 20 04 47    OK
	                                          M  A  14                S  a  u  g

	18< MA.I....Zuluftgebl.2                  4D 41 0F 49 00 1E 03 07 5A 75 6C 75 66 74 67 65 62 6C 05 32    OK
	                                          M  A  15                Z  u  l  u  f  t

	20< MA.I....Einschub.9                    4D 41 0D 49 00 1B 03 06 45 69 6E 73 63 68 75 62 04 39    OK
	                                          M  A  13                E  i  n  s  c  h

	22< MA.I.,./Fuellst.:.?                   4D 41 0E 49 00 2C 03 2F 46 75 65 6C 6C 73 74 2E 3A 04 8A    OK
	                                          M  A  14                F  u  e  l  l

	24< MA.I....Feuerraumt..                  4D 41 0F 49 00 04 07 0B 46 65 75 65 72 72 61 75 6D 74 05 1C    OK
	                                          M  A  15                F  e  u  e  r

	26< MA.I....Boilertemp..                  4D 41 0F 49 00 02 00 10 42 6F 69 6C 65 72 74 65 6D 70 05 0B    OK
	                                          M  A  15                B  o  i  l

	28< MA.I....Au�entemp.,                   4D 41 0E 49 00 0A 00 1D 41 75 E1 65 6E 74 65 6D 70 05 2C    OK
	                                          M  A  14                A  u  s

	30< MA.I....Vorlauft.1sw.�                4D 41 11 49 00 15 03 1E 56 6F 72 6C 61 75 66 74 2E 31 73 77 05 BA    OK
	                                          M  A  17                V  o  r  l

	32< MA.I....Vorlauft.1.�                  4D 41 0F 49 00 0B 01 1F 56 6F 72 6C 61 75 66 74 2E 31 04 C3    OK
	                                          M  A  15                V  o  r  l

	34< MA.I.).(KTY6_H2.B                     4D 41 0C 49 00 29 07 28 4B 54 59 36 5F 48 32 03 42    OK
	                                          M  A  12                K  T  Y

	36< MA.I.*.)KTY7_H2.E                     4D 41 0C 49 00 2A 07 29 4B 54 59 37 5F 48 32 03 45    OK
	                                          M  A  12                K  T  Y

	38< MA.I.0.3Brenn.ST..                    4D 41 0D 49 00 30 05 33 42 72 65 6E 6E 2E 53 54 04 16    OK
	                                          M  A  13                B  r  e  n

	40< MA.I...&Laufzeit:.?                   4D 41 0E 49 00 12 01 26 4C 61 75 66 7A 65 69 74 3A 04 9C    OK
	                                          M  A  14                L  a  u  f

	42< MA.I...,Boardtemp..�                  4D 41 0F 49 00 18 01 2C 42 6F 61 72 64 74 65 6D 70 2E 04 F7    OK
	                                          M  A  15                B  o  a

	44< MA.I...-Die.Kesseltemp..soll.sein.N   4D 41 1E 49 00 17 05 2D 44 69 65 20 4B 65 73 73 65 6C 74 65 6D 70 2E 0A 73 6F 6C 6C 20 73 65 69 6E 0A 4E    OK
	                                          M  A  30                D  i  e     K  e  s  s  e  l  t  e  m  p  .

	*/

	// statemachine
	// --------------------
	do {
		switch(tCurrentMainState)
		{
			case STM_INIT:
			{
				// init globals
				glsUserID = -7;
				ValueCount = 0;

				// init locals
				k = 0;
				iInBuffPointer = 0;
				cRbSendFlag = 0;
				iMsgLength = 0;

				//ask for Com port number to open, if not given by argv
				if (argc<2) {
					printf("ComPort-Nummer [1..6]: ");
					scanf("%i", &iPortid); }
				else { iPortid = atoi(argv[1]); }
				iPortid--;
				printf("Try to open ComPort: %i\n", iPortid+1);
				OpenComPort(iPortid, 9600);

				printf("Ready to receive from ComPort: %i\n", iPortid + 1);
				printf("Press CTRL-C to abort\n");

				// init communication by sendig "Ra" to P3100
				if (DEBUG==1) printf("vor Send_Msg Ra\n");
				Send_Msg("Ra");
				if (DEBUG==1) printf("nach Send_Msg Ra\n");
				// next states
				tCurrentSubState = STM_SUB_RECEIVE_HEADER;
				tCurrentMainState = STM_RECEIVE_MSG;

				break;
			}
			case STM_RECEIVE_MSG:
			{
				// read one character from the input buffer
				if (ComReceive(&cReceiveBuffer[iInBuffPointer],1) > 0) {
					iInBuffPointer++;
					if (DEBUG>0) printf("iInBuffPointer: %d\n",iInBuffPointer);
					
				}

				switch(tCurrentSubState)
				{
					case STM_SUB_RECEIVE_HEADER:
					{
						if(iInBuffPointer >= 3){
							// get parameter length
							iMsgLength = (unsigned int)cReceiveBuffer[2] + 5;
							if ((iMsgLength) > RECEIVE_BUF_SIZE){
								PrintComLog(LOG_ERR, "Buffersize to small for telegram", 32);
								iInBuffPointer = 0;
								iMsgLength = 0;
							}
							else if (cReceiveBuffer[0] == 'R'){
								// set next state
								tCurrentSubState = STM_SUB_RECEIVE_REPLY_BODY;
							}
							else if (cReceiveBuffer[0] == 'M'){
								// set next state
								tCurrentSubState = STM_SUB_RECEIVE_MESSAGE_BODY;
							}
							else{
								PrintComLog(LOG_ERR, "Unvalid telegram startvalue", 27);
								iInBuffPointer = 0;
							}
						}
						break;
					}

					case STM_SUB_RECEIVE_REPLY_BODY:
					{
						// wait until 6 bytes have been received
						if(iInBuffPointer >= 6){

							// check received checksum
							if(checkCheckSum(cReceiveBuffer, 6)){
								PrintComLog(LOG_INFO, "ChkSum OK", 9);
								printf("\n");
							}
							else{
								PrintComLog(LOG_INFO, "ChkSum NOT OK", 13);
							}
							iInBuffPointer = 0;
							tCurrentSubState = STM_SUB_RECEIVE_HEADER;
						}
						break;
					}
					case STM_SUB_RECEIVE_MESSAGE_BODY:
					{
						// is telegram complete?
						if(iInBuffPointer >= iMsgLength){

							// check received checksum
							if(checkCheckSum(cReceiveBuffer, iMsgLength)){
								//PrintComLog(LOG_INFO, "ChkSum OK", 9);
								//printf("\n");
								//usleep(250 * 1000);
								sendAcq(cReceiveBuffer, iMsgLength);
							}
							else{
								PrintComLog(LOG_INFO, "ChkSum NOT OK", 13);
								sendNAcq(cReceiveBuffer, iMsgLength);
							}

							// check if value names have been received (MA messages)
							if(cReceiveBuffer[1] == 'A'){
								tCurrentMainState = STM_HANDLE_MA;
							}

							// check if measurement values have been received (M1 messages)
							else if(cReceiveBuffer[1] == '1'){
								tCurrentMainState = STM_HANDLE_M1;
							}

							// check if date/time has been received
							else if(cReceiveBuffer[1] == '2'){
								tCurrentMainState = STM_HANDLE_M2;
							}
							// check if I frame has been received
							else if(cReceiveBuffer[1] == 'I'){
								tCurrentMainState = STM_HANDLE_MI;
							}
							else
							{
								PrintComLog(LOG_RECEIVE, cReceiveBuffer, iMsgLength);

								// next states
								tCurrentSubState = STM_SUB_RECEIVE_HEADER;
								tCurrentMainState = STM_RECEIVE_MSG;
							}
							iInBuffPointer = 0;
						}
						break;
					}
				}
				break;
			}

			case STM_HANDLE_MA:
			{
				ValueNames[ValueCount] = (char*)malloc( (iMsgLength - 9)* sizeof(char));
				memcpy(ValueNames[ValueCount], &cReceiveBuffer[8], iMsgLength - 10);
				ValueNames[ValueCount][iMsgLength - 10] = '\0';
				ValueCount++;

				PrintComLog(LOG_RECEIVE, cReceiveBuffer, iMsgLength);

				// next states
				tCurrentSubState = STM_SUB_RECEIVE_HEADER;
				tCurrentMainState = STM_RECEIVE_MSG;

				break;
			}
			case STM_HANDLE_M1:
			{
				int i;
				int startindex = 3;
				float sValue = 0;

				// fuer Ausgabe am Bildschirm
				if (Output2Stdout) {
				printf("%s", cTime);
				for(i = 0; i < ValueCount; i++)
				{
					sValue = (float)(((unsigned char)(cReceiveBuffer[startindex + (i*2)]) << 8) + (unsigned char)cReceiveBuffer[startindex + (i*2 + 1)]);

					// scale some values
					switch(i){
						case  4: {sValue = sValue / 2.0   ;break;}
						case 11: {sValue = sValue / 10.0  ;break;}
						case 13: {sValue = sValue / 207.0 ;break;}
						case 15: {sValue = sValue / 2.0   ;break;}
						case 16: {sValue = sValue / 2.0   ;break;}
						//case 18: {sValue = sValue / 2.0   ;break;}
						//case 19: {sValue = sValue / 2.0   ;break;}
						//case 20: {sValue = sValue / 2.0   ;break;}
						case 21: {sValue = sValue / 2.0 ; break;}
						case 22: {sValue = sValue / 2.0;break;}
						case 29: {sValue = sValue / 2.0 ; break;}
							}
					// anlagenspezifischer hack, da ValueName[29] wohl \n enthaelt
					if (i==21) printf("%2d. %15s :   %7.1f\n",i,"Kesselsolltemp.",sValue);
					else printf("%2d. %15s :   %7.1f\n",i,ValueNames[i],sValue);
				}
 				LinesUp(ValueCount+3); // steigt ValueCount+3 Zeilen nach oben
				}
				// Ende Bildschirmausgabe
				
				// Beginn Ausgabe in OutputFile
				if (OutputFileCounter==OutputFileDelay) {
					for(i = 0; i < ValueCount; i++) {
						sValue = (short)(((unsigned char)(cReceiveBuffer[startindex + (i*2)]) << 8) + (unsigned char)cReceiveBuffer[startindex + (i*2 + 1)]);

						// scale some values
						switch(i){
							case 4: {sValue = sValue / 2.0 ;break;}
							case 11: {sValue = sValue / 10.0 ;break;}
							case 13: {sValue = sValue / 207.0 ;break;}
							case 15: {sValue = sValue / 2.0 ;break;}
							case 16: {sValue = sValue / 2.0 ;break;}
							//case 18: {sValue = sValue / 2.0 ;break;}
							//case 19: {sValue = sValue / 2.0 ;break;}
						        //case 20: {sValue = sValue /2.0 ; break;}
							case 21: {sValue = sValue /2.0 ; break;}
							case 22: {sValue = sValue / 2.0;break;}
							case 29: {sValue = sValue / 2.0 ; break;}
						}
						// fuegt kuenstliche Namen fuer namenlose Messwerte ein
						if (i==0)  fprintf(OutputFile,"%2d;%15s ; %7.1f\n",i,"Zustand 0",sValue);
						else { if (i==1) fprintf(OutputFile,"%2d;%15s ; %7.1f\n",i,"Zustand 1",sValue);				
							else { if (i==21) fprintf(OutputFile,"%2d;%15s ; %7.1f\n",i,"Kesselsolltemp.",sValue);
								else fprintf(OutputFile,"%2d;%15s ; %7.1f\n",i,ValueNames[i],sValue);}}
					}
					fclose(OutputFile);
					//system("cp /home/pi/froeling_p3100_logger/current_data.txt /var/www");
					OutputFileCounter=0;
				}
				// Ende Ausgabe OutputFile
				
				// Beginn Dateiausgabe in Logfile
				if (DataFileCounter==DataFileDelay) {
				//DataFile=fopen(DataFileName,"a"); // oeffnet Datei, anhaengen, ggf. erzeugen
				for(i = 0; i < ValueCount; i++) {
					sValue = (short)(((unsigned char)(cReceiveBuffer[startindex + (i*2)]) << 8) + (unsigned char)cReceiveBuffer[startindex + (i*2 + 1)]);

					// scale some values
					switch(i){
						case 4: {sValue = sValue / 2.0 ;break;}
						case 11: {sValue = sValue / 10.0 ;break;}
						case 13: {sValue = sValue / 207.0 ;break;}
						case 15: {sValue = sValue / 2.0 ;break;}
						case 16: {sValue = sValue / 2.0 ;break;}
						//case 18: {sValue = sValue / 2.0 ;break;}
						//case 19: {sValue = sValue / 2.0 ;break;}
						//case 20: {sValue = sValue /2.0 ; break;}
						case 21: {sValue = sValue /2.0 ; break;}
						case 22: {sValue = sValue / 2.0;break;}
						case 29: {sValue = sValue / 2.0 ; break;}
					}
					fprintf(DataFile,";%5.1f", sValue);} // Hier Datenausgabe formatieren
				fprintf(DataFile,"\n");
				fclose(DataFile);
				DataFileCounter=0;}
				// Ende Dateiausgabe Logfile

				// next states
				tCurrentSubState = STM_SUB_RECEIVE_HEADER;
				tCurrentMainState = STM_RECEIVE_MSG;
				break;
			}
			case STM_HANDLE_M2:
			{
				if(cRbSendFlag == 0){
					// init measurement value transfer by sending "Rb" to P3100
					Send_Msg("Rb");
					cRbSendFlag = 1;
					//PrepareScreen();

					//Set time on P3100
					//Set_Time();

					//Set workmode on P3100
					//Set_ParamValue(FROE_PARAM_WORKMODE, FROE_WORKMODE_REINIGEN);
				}
				
				if (Output2Stdout) {
				sprintf(cTime, "\nAnlagendatum: %02i.%02i.20%02i  Anlagenzeit: %02i:%02i:%02i\n\n", \
				convert_FroelingTime(cReceiveBuffer[6]),\
				convert_FroelingTime(cReceiveBuffer[7]),\
				convert_FroelingTime(cReceiveBuffer[9]),\
				convert_FroelingTime(cReceiveBuffer[5]),\
				convert_FroelingTime(cReceiveBuffer[4]),\
				convert_FroelingTime(cReceiveBuffer[3]));
				}

				// Beginn Ausgabe in OuputFile (letzte Messwerte in einer Datei)
				OutputFileCounter++;
				if (OutputFileCounter==OutputFileDelay) {
					OutputFile=fopen(ACTUALDATA,"w");
					fprintf(OutputFile, "-2  ;   Anlagendatum ; %02i.%02i.20%02i\n-1  ;    Anlagenzeit ; %02i:%02i:%02i\n", \
					convert_FroelingTime(cReceiveBuffer[6]),\
					convert_FroelingTime(cReceiveBuffer[7]),\
					convert_FroelingTime(cReceiveBuffer[9]),\
					convert_FroelingTime(cReceiveBuffer[5]),\
					convert_FroelingTime(cReceiveBuffer[4]),\
					convert_FroelingTime(cReceiveBuffer[3]));
				}
				// Ende Ausgabe in OutputFile (ohne Datei zu schliessen)
				
				// fuer Dateiausgabe
				DataFileCounter++;
				//printf("DataFileCounter: %d   DataFileDelay: %d",DataFileCounter,DataFileDelay);
				if (DataFileCounter==DataFileDelay) {
				SetDataFileName(); // setze Dateinamen nach aktuellem Datum, damit fuer jeden Tag ein neues logfile erstellt wird
				if (DEBUG>0) printf("Oeffne logdatei %s\n",DataFileName);
				DataFile=fopen(DataFileName,"a");
				fprintf(DataFile, "20%02i-%02i-%02i; %02i:%02i:%02i", \
				convert_FroelingTime(cReceiveBuffer[9]),\
				convert_FroelingTime(cReceiveBuffer[7]),\
				convert_FroelingTime(cReceiveBuffer[6]),\
				convert_FroelingTime(cReceiveBuffer[5]),\
				convert_FroelingTime(cReceiveBuffer[4]),\
				convert_FroelingTime(cReceiveBuffer[3]));
				//fclose(DataFile);
				}
				// Ende Dateiausgabe

				// fuege Wartezeit ein, um Prozessorauslastung zu verringern, muss ans System angepasst werden
				usleep(900 * 1000);
				// next states
				tCurrentSubState = STM_SUB_RECEIVE_HEADER;
				tCurrentMainState = STM_RECEIVE_MSG;

				break;
			}
			case STM_HANDLE_MI:
			{
				PrintComLog(LOG_RECEIVE, cReceiveBuffer, iMsgLength);

				// next states
				tCurrentSubState = STM_SUB_RECEIVE_HEADER;
				tCurrentMainState = STM_RECEIVE_MSG;

				break;
			}
		}

		} while (cKb != ESC);

		// cleanup
#ifdef WINDOWS
	CloseHandle(hCom);
#endif

#ifdef LINUX
	close(hCom);
#endif
		for(i = 0; i < ValueCount; i++)
		{
			free(ValueNames[i]);
		}
	   return (0);
}

/* ---------------------------------------------------
 * Open com Port
 * ---------------------------------------------------
 */
int  OpenComPort(int PortId, int BaudRate)
{
#ifdef WINDOWS
	DCB dcb; /* device control block */
	int fSuccess;
	COMMTIMEOUTS timeouts;
	char *ComPort[] = {"COM1","COM2","COM3","COM4","COM5","COM6"};

	// open port
	hCom = CreateFile(	ComPort[PortId],
				GENERIC_READ | GENERIC_WRITE,
				0,
				NULL,
				OPEN_EXISTING,
				0, /* no overlapped I/O */
				NULL); /* must be NULL for comm devices */

	if(hCom == INVALID_HANDLE_VALUE){
		return(-1);
	}

	fSuccess = GetCommState(hCom, &dcb);

	/* configure the port */
	dcb.BaudRate = BaudRate;
	dcb.ByteSize = 8;
	dcb.Parity = NOPARITY;
	dcb.StopBits = ONESTOPBIT;
	dcb.fDtrControl = DTR_CONTROL_DISABLE;
	dcb.fInX = FALSE;
	fSuccess = SetCommState(hCom, &dcb);

	fSuccess = GetCommTimeouts (hCom, &timeouts);

	timeouts.ReadIntervalTimeout = MAXDWORD;
	timeouts.ReadTotalTimeoutMultiplier = 0;
	timeouts.ReadTotalTimeoutConstant = 20; // wait 20 ms
	fSuccess = SetCommTimeouts (hCom, &timeouts);
#endif

#ifdef LINUX
	struct termios options;
	char *ComPort[] = {"/dev/ttyS0","/dev/ttyS1","/dev/ttyUSB0","/dev/ttyUSB1","/dev/ttyS3","/dev/ttyS4"};

	printf("PortID %d  Datei: %s\nFargv",PortId, ComPort[PortId]);
	sleep (3);
	hCom = open(ComPort[PortId], O_RDWR | O_NOCTTY | O_NDELAY);
	sleep(1);
	if (hCom == -1){
		return(-1);
	}

	// Get the current options for the port...
	tcgetattr(hCom, &options);
	
	//fcntl(hCom, F_SETFL, 0);
        if (tcgetattr(hCom, &options) != 0) return(-1);
        bzero(&options, sizeof(options)); /* Structure loeschen, ggf vorher sichern
                                               und bei Programmende wieder restaurieren */

	// Set the baud rates to 9600...
	/*cfsetispeed(&options, B9600);
	cfsetospeed(&options, B9600); */
	cfsetspeed(&options, B9600);

	// Enable the receiver and set local mode...
	options.c_oflag = 0;
	options.c_lflag = 0;

	// switch off CR changed to LF
	options.c_iflag &= ~ICRNL;

	// Set 8N1
	options.c_cflag &= ~PARENB;   /* kein Paritybit */
	options.c_cflag &= ~CSTOPB;   /* 1 Stoppbit */
	options.c_cflag &= ~CSIZE;    /* 8 Datenbits */
	options.c_cflag |= CS8;
	options.c_cflag |= (CLOCAL | CREAD);/* CD-Signal ignorieren */ 
	/* Kein Echo, keine Steuerzeichen, keine Interrupts */
	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	options.c_oflag &= ~OPOST;          /* setze "raw" Input */
	options.c_cc[VMIN]  = 0;            /* warten auf min. 0 Zeichen */
	options.c_cc[VTIME] = 10;           /* Timeout 1 Sekunde */

	tcflush(hCom, TCIFLUSH);

	// Set the new options for the port...
	 if (tcsetattr(hCom, TCSAFLUSH, &options) != 0) return(-1);
#endif

	return(hCom);
}

/* ---------------------------------------------------
 * Send something via ComPort
 * ---------------------------------------------------
 */
int  ComSend(char* SendBuffer, int bytes2send)
{
	long BytesWrite;
#ifdef WINDOWS
	WriteFile( hCom, SendBuffer, bytes2send, &BytesWrite, NULL);
#endif

#ifdef LINUX
	BytesWrite = write(hCom, SendBuffer, bytes2send);
	if (DEBUG==1) printf("BytelsWrite: %d\n",(int)BytesWrite);
	if(BytesWrite < 0){
		return(-1);
	}
#endif
	return(0);
}

/* ---------------------------------------------------
 * Receive something via ComPort
 * ---------------------------------------------------
 */
int  ComReceive(char* ReceiveBuffer, int bytes2receive)
{
	long BytesRead;

#ifdef WINDOWS
	ReadFile ( hCom, ReceiveBuffer, bytes2receive, &BytesRead, NULL);
#endif

#ifdef LINUX
	BytesRead = read(hCom, ReceiveBuffer, bytes2receive);
#endif
	if (DEBUG>0) printf("fuehre ComReceive aus: Anzahl gelesener Bytes: %ld / gelesene Zeichen: %s\n", BytesRead,ReceiveBuffer);
	if (BytesRead<1) {
		if (DEBUG>0) {printf("Fuehre Send_Masg(\"Ra\"); zusaetzlich aus!\n");
		Send_Msg("Ra");}
	}	
	return BytesRead;
}

/* ---------------------------------------------------
 * Construct and send a message in Froeling-Format
 * ---------------------------------------------------
 */
void Send_Msg(char* msg)
{
	char sendbuffer[8];
	char arrayUserID[3];
	char b[8];
	char cs[2];

	arrayUserID[0] = 0;
	arrayUserID[1] = (char)((glsUserID >> 8) & 0xFF);
	arrayUserID[2] = (char)(glsUserID & 0xFF);

	memset(b, 0,(8)*sizeof(char));

	b[0] = msg[0];
	b[1] = msg[1];
	b[2] = 3;

	memcpy(&b[3], arrayUserID, 3);
	CreateCheckSum(b, 6, cs);
	memcpy(&b[6], cs, 2);
	memcpy(sendbuffer, b, 8);

	PrintComLog(LOG_SEND, sendbuffer, 8);

	if (DEBUG>0) printf("vor ComSend\n");
	ComSend(sendbuffer, 8);
	if (DEBUG>0) printf("nach ComSend\n");
	return;
}

/* ---------------------------------------------------
 * Set time on P3100
 * ---------------------------------------------------
 */
void Set_Time(void)
{
	time_t      seconds;
	struct tm*  timeinfo;

	// message prototype
	//                   R     2    len   sec  min   hour  day  month  dow   year | Checksum
	char sendbuffer[]={0x52, 0x32, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	char cs[2];

    // get the systemtime
    seconds = time(NULL);

    // convert seconds from 1970 into tm - structure
    timeinfo = (struct tm*)localtime(&seconds);

    sendbuffer[8] = convert_2_FroelingTime(timeinfo->tm_wday);
    sendbuffer[6] = convert_2_FroelingTime(timeinfo->tm_mday);
    sendbuffer[7] = convert_2_FroelingTime(timeinfo->tm_mon + 1);
    sendbuffer[9] = convert_2_FroelingTime(timeinfo->tm_year - 100);
    sendbuffer[5] = convert_2_FroelingTime(timeinfo->tm_hour);
    sendbuffer[4] = convert_2_FroelingTime(timeinfo->tm_min);
    sendbuffer[3] = convert_2_FroelingTime(timeinfo->tm_sec);

	CreateCheckSum(sendbuffer, 10, cs);
	sendbuffer[10] = cs[0];
	sendbuffer[11] = cs[1];

	PrintComLog(LOG_SEND, sendbuffer, 12);
	ComSend(sendbuffer, 12);
	return;
}

/* ---------------------------------------------------
 * Set a parameter on P3100
 * ---------------------------------------------------
 */
void Set_ParamValue(T_FROE_PARAMID ParamID, int value)
{
	// message prototype
	//                   R     I    len | ParamID   |  Value    | Checksum
	char sendbuffer[]={0x52, 0x49, 0x04, 0x00, 0x9b, 0x00, 0x00, 0x00, 0x00};
	char cs[2];

	// set ParamID in msg prototype
	sendbuffer[3] = (char)((ParamID >> 8) & 0xFF);
	sendbuffer[4] = (char)(ParamID & 0xFF);

	// set mode in msg prototype
	sendbuffer[5] = (char)((value >> 8) & 0xFF);
	sendbuffer[6] = (char)(value & 0xFF);

	CreateCheckSum(sendbuffer, 7, cs);
	sendbuffer[7] = cs[0];
	sendbuffer[8] = cs[1];

	PrintComLog(LOG_SEND, sendbuffer, 9);
	ComSend(sendbuffer, 9);
	return;
}

/* ---------------------------------------------------
 * Create a Froeling Checksum
 * ---------------------------------------------------
 */
void CreateCheckSum(char* buf, int buffsize, char* checksum)
{
    unsigned short cs = 0;
    int i;

    for (i = 0; i < buffsize; i++){
        cs += (unsigned char)buf[i];
    }
    checksum[0] = (unsigned char)(cs >> 8);
    checksum[1] = (unsigned char) cs;
    return;
}

/* ---------------------------------------------------
 * Check if a checksum is correct
 * ---------------------------------------------------
 */
int checkCheckSum(char* buf, int buffsize)
{
	int i;
	int result = 0;
    unsigned short cs1 = 0;
    unsigned short cs2 = 0;

    // take last 2 bytes from received string (received checksum)
    cs1 = (unsigned short)(((unsigned char)buf[buffsize - 2] * 256) + (unsigned char)buf[buffsize - 1]);

    // build own checksum from received string
    for (i = 0; i < (buffsize - 2); i++){
        cs2 += (unsigned char)buf[i];
    }

    // compare both values
    if(cs1 == cs2){
    	result = 1;
    }
    return result;
}

/* ---------------------------------------------------
 * Send a Froeling acknowledge
 * ---------------------------------------------------
 */
void sendAcq(char* buf, int buffsize)
{
	char b[6];
	char cs[2];

    if (buffsize > 1)
    {
        b[0] = buf[0];
        b[1] = buf[1];
        b[2] = 1;
        b[3] = 1;
        b[4] = 0;
        b[5] = 0;
        CreateCheckSum(b, 6, cs);
        memcpy(&b[4], cs, 2);
    }
    ComSend(b, 6);
    return;
}

/* ---------------------------------------------------
 * Send a "Not acknowledge" to Froeling
 * ---------------------------------------------------
 */
void sendNAcq(char* buf, int buffsize)
{
	char b[6];
	char cs[2];

    if (buffsize > 1)
    {
        b[0] = buf[0];
        b[1] = buf[1];
        b[2] = 1;
        b[3] = 0;
        b[4] = 0;
        b[5] = 0;
        CreateCheckSum(b, 6, cs);
        memcpy(&b[4], cs, 2);
    }
    PrintComLog(LOG_SEND, b, 6);
    ComSend(b, 6);
    return;
}

/* ---------------------------------------------------
 * Print a debug message to console
 * ---------------------------------------------------
 */
void PrintComLog(T_LOG_IDENT logident, char* buffer, int bufferlength)
{
	//todo	time_t	date;
	//todo	struct	timeb tstruct;
	char	stream[2048];
	char    tmp[64];
	int 	i;

	// get actual date and time
	//todo	date = time(NULL);
	//todo	ftime( &tstruct );

	// build date/time string
	//if (DEBUG==3) printf("Bildschirmausgabe\n");
	switch(logident)
	{
		case LOG_SEND:
			sprintf(stream, "<<< ");
			break;

		case LOG_RECEIVE:
			sprintf(stream, ">>> ");
			break;

		case LOG_ERR:
			sprintf(stream, "ERR ");
			break;

		case LOG_INFO:
			sprintf(stream, "INF ");
			break;
	}

	// print buffer to stdio
	for(i = 0; i < bufferlength; i++){
		if(((buffer[i]>=48) && (buffer[i]<=57)) || \
		   ((buffer[i]>=65) && (buffer[i]<=90)) || \
		   ((buffer[i]>=97) && (buffer[i]<=122)))
			strncat(stream, &buffer[i], 1);
	}

	// print an "ASCII dump"
	if((logident == LOG_SEND)||(logident == LOG_RECEIVE))
	{
		strcat(stream, " {");

		for(i = 0; i < bufferlength; i++){
			sprintf(tmp, "%02x ", (unsigned char)buffer[i]);
			strcat(stream, tmp);
		}
		strcat(stream, "}\n \0");
	}

	printf("%s", stream);

	return;
}

/* ---------------------------------------------------
 * set cursor to screen position x,y
 * ---------------------------------------------------
 */
void GotoXY(int x, int y)
{
	#ifdef LINUX
		printf("\033[%02d;%02dH", y,x);
	#endif 
}

/* ---------------------------------------------------
 * convert Froeling Date/Time segment to an integer value
 * Froeling interprets a Hex value directly as integer...
 * 0x12 means hour = 12; 0x55 means minutes = 55
 * ---------------------------------------------------
 */
int convert_FroelingTime(char x)
{
	int y = 0;

	y = (int)(x>>4);
	y = (y*10) + (x-(y<<4));
	return(y);
}

char convert_2_FroelingTime(int x)
{
	char t = 0;
	char y = 0;

	t = ((int)(x/10));
	y = t*16 + (x-(t*10));
	return(y);
}

/* ---------------------------------------------------
 * Prepare the main console screen
 * ---------------------------------------------------
 */
void PrepareScreen(void)
{
	int i;

	// clear screen
	#ifdef WINDOWS
		system("cls");
	#endif

	#ifdef LINUX
		printf("\033[2J\033[1;1H"); // clearscreen
	#endif

	// headline
	printf(
	"\n*********************************************************************\n"
	"*     Froeling P3100 communication test (Build 0),                  *\n"
	"*     Markus Kotschenreuther, 2014                                  *\n"
	"*********************************************************************\n\n");

	printf("Press (CTRL + C) to abort\n");

	GotoXY(0, 10);
	printf("Name");
	GotoXY(20, 10);
	printf("Value");
	GotoXY(0, 11);
	printf("-----------------------------------------------------------");

	for(i = 0; i < ValueCount; i++)
	{
		GotoXY(0, i + 12);
		printf("%s", ValueNames[i]);
	}
	return;
}

/* --------------------------------------------------------
* steigt eine bestimmete Anzahl von Zeilen zurueck
* ---------------------------------------------------------
*/
void LinesUp(int nrows)
{
	int i=0;
	#ifdef LINUX
		for (i=0; i<nrows; i++) printf("\033[A\033[2K"); 
	#endif
}

void SetDataFileName(void)
{
	time(&tnow);
   	tmnow = localtime(&tnow);
   	sprintf(DataFileName,"./logfiles/froeling_data_%4i-%02i-%02i.log",tmnow->tm_year + 1900,tmnow->tm_mon + 1,tmnow->tm_mday);
}
