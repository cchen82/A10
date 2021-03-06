/**************************************************************************//**
* @file      CliThread.c
* @brief     File for the CLI Thread handler. Uses FREERTOS + CLI
* @author    Eduardo Garcia
* @date      2020-02-15

******************************************************************************/


/******************************************************************************
* Includes
******************************************************************************/
#include "CliThread.h"
#include "IMU/lsm6ds_reg.h"
#include "SeesawDriver/Seesaw.h"
#include "OLED_driver/OLED_driver.h"

/******************************************************************************
* Defines
******************************************************************************/


/******************************************************************************
* Variables
******************************************************************************/
static const int8_t * const pcWelcomeMessage =
"FreeRTOS CLI.\r\nType Help to view a list of registered commands.\r\n";


static const CLI_Command_Definition_t xImuGetCommand =
{
	"imu",
	"imu: Returns a value from the IMU\r\n",
	CLI_GetImuData,
	0
};


static const CLI_Command_Definition_t xResetCommand =
{
	"reset",
	"reset: Resets the device\r\n",
	CLI_ResetDevice,
	0
};

static const CLI_Command_Definition_t xNeotrellisTurnLEDCommand =
{
	"led",
	"led [keynum][R][G][B]: Sets the given LED to the given R,G,B values.\r\n",
	CLI_NeotrellisSetLed,
	4
};

static const CLI_Command_Definition_t xOLEDdrawCircleCommand =
{
	"draw",
	"draw [x][y][radius][color]: draw a circle on OLED.\r\n color selection: (0) black (1) white\r\n",
	CLI_OLEDdrawCircle,
	4
};


//Clear screen command
const CLI_Command_Definition_t xClearScreen =
{
	CLI_COMMAND_CLEAR_SCREEN,
	CLI_HELP_CLEAR_SCREEN,
	CLI_CALLBACK_CLEAR_SCREEN,
	CLI_PARAMS_CLEAR_SCREEN
};


/******************************************************************************
* Forward Declarations
******************************************************************************/

/******************************************************************************
* Callback Functions
******************************************************************************/


/******************************************************************************
* CLI Thread
******************************************************************************/

void vCommandConsoleTask( void *pvParameters )
{
//REGISTER COMMANDS HERE

FreeRTOS_CLIRegisterCommand( &xImuGetCommand );
FreeRTOS_CLIRegisterCommand( &xClearScreen );
FreeRTOS_CLIRegisterCommand( &xResetCommand );
FreeRTOS_CLIRegisterCommand( &xNeotrellisTurnLEDCommand );
FreeRTOS_CLIRegisterCommand( &xOLEDdrawCircleCommand );

uint8_t cRxedChar[2], cInputIndex = 0;
BaseType_t xMoreDataToFollow;
/* The input and output buffers are declared static to keep them off the stack. */
static int8_t pcOutputString[ MAX_OUTPUT_LENGTH_CLI  ], pcInputString[ MAX_INPUT_LENGTH_CLI ];
static char pcLastCommand[ MAX_INPUT_LENGTH_CLI ];
static bool isEscapeCode = false;
static char pcEscapeCodes [4];
static uint8_t pcEscapeCodePos = 0;

    /* This code assumes the peripheral being used as the console has already
    been opened and configured, and is passed into the task as the task
    parameter.  Cast the task parameter to the correct type. */

    /* Send a welcome message to the user knows they are connected. */
    SerialConsoleWriteString( pcWelcomeMessage);

    for( ;; )
    {
        /* This implementation reads a single character at a time.  Wait in the
        Blocked state until a character is received. */
        int recv = SerialConsoleReadCharacter(&cRxedChar);
		if(recv == -1) //If no characters in the buffer, thread goes to sleep for a while
		{
			vTaskDelay( CLI_TASK_DELAY);
		}else if( cRxedChar[0] == '\n' || cRxedChar[0] == '\r'  )
        {
            /* A newline character was received, so the input command string is
            complete and can be processed.  Transmit a line separator, just to
            make the output easier to read. */
            SerialConsoleWriteString("\r\n");
			//Copy for last command
			isEscapeCode = false; pcEscapeCodePos = 0;
			strncpy(pcLastCommand, pcInputString, MAX_INPUT_LENGTH_CLI-1);
			pcLastCommand[MAX_INPUT_LENGTH_CLI-1] = 0;	//Ensure null termination

            /* The command interpreter is called repeatedly until it returns
            pdFALSE.  See the "Implementing a command" documentation for an
            explanation of why this is. */
            do
            {
                /* Send the command string to the command interpreter.  Any
                output generated by the command interpreter will be placed in the
                pcOutputString buffer. */
                xMoreDataToFollow = FreeRTOS_CLIProcessCommand
                              (
                                  pcInputString,   /* The command string.*/
                                  pcOutputString,  /* The output buffer. */
                                  MAX_OUTPUT_LENGTH_CLI/* The size of the output buffer. */
                              );

                /* Write the output generated by the command interpreter to the
                console. */
				//Ensure it is null terminated
				pcOutputString[MAX_OUTPUT_LENGTH_CLI - 1] = 0;
                SerialConsoleWriteString(pcOutputString);

            } while( xMoreDataToFollow != pdFALSE );

            /* All the strings generated by the input command have been sent.
            Processing of the command is complete.  Clear the input string ready
            to receive the next command. */
            cInputIndex = 0;
            memset( pcInputString, 0x00, MAX_INPUT_LENGTH_CLI );
        }
        else
        {
		            /* The if() clause performs the processing after a newline character
            is received.  This else clause performs the processing if any other
            character is received. */
		
			if (true == isEscapeCode) {

				if(pcEscapeCodePos < CLI_PC_ESCAPE_CODE_SIZE) {
					pcEscapeCodes[pcEscapeCodePos++] = cRxedChar[0];
				}
				else {
					isEscapeCode = false; pcEscapeCodePos = 0;
				}
			
				if (pcEscapeCodePos >= CLI_PC_MIN_ESCAPE_CODE_SIZE) {
				
					// UP ARROW SHOW LAST COMMAND
					if(strcasecmp(pcEscapeCodes, "oa"))	{
                            /// Delete current line and add prompt (">")
                            sprintf(pcInputString, "%c[2K\r>", 27);
				            SerialConsoleWriteString(pcInputString);
                            /// Clear input buffer
                            cInputIndex = 0;
                            memset( pcInputString, 0x00, MAX_INPUT_LENGTH_CLI );
                        /// Send last command
						strncpy(pcInputString, pcLastCommand, MAX_INPUT_LENGTH_CLI - 1); 	
                        cInputIndex = (strlen(pcInputString) < MAX_INPUT_LENGTH_CLI - 1) ? strlen(pcLastCommand) : MAX_INPUT_LENGTH_CLI - 1;
						SerialConsoleWriteString(pcInputString);
					}
				
					isEscapeCode = false; pcEscapeCodePos = 0;
				}			
			}
            /* The if() clause performs the processing after a newline character
            is received.  This else clause performs the processing if any other
            character is received. */

            else if( cRxedChar[0] == '\r' )
            {
                /* Ignore carriage returns. */
            }
            else if( cRxedChar[0] == ASCII_BACKSPACE || cRxedChar[0] == ASCII_DELETE )
            {
				char erase[4] = {0x08, 0x20, 0x08, 0x00};
				SerialConsoleWriteString(erase);
                /* Backspace was pressed.  Erase the last character in the input
                buffer - if there are any. */
                if( cInputIndex > 0 )
                {
                    cInputIndex--;
                    pcInputString[ cInputIndex ] = 0;
                }
            }
			// ESC
			else if( cRxedChar[0] == ASCII_ESC) {
				isEscapeCode = true; //Next characters will be code arguments
				pcEscapeCodePos = 0;
			}
            else
            {
                /* A character was entered.  It was not a new line, backspace
                or carriage return, so it is accepted as part of the input and
                placed into the input buffer.  When a n is entered the complete
                string will be passed to the command interpreter. */
                if( cInputIndex < MAX_INPUT_LENGTH_CLI )
                {
                    pcInputString[ cInputIndex ] = cRxedChar[0];
                    cInputIndex++;
                }

					//Order Echo
					cRxedChar[1] = 0;
					SerialConsoleWriteString(&cRxedChar[0]);
            }
        }
    }
}




/******************************************************************************
* CLI Functions - Define here
******************************************************************************/
//Example CLI Command. Reads from the IMU and returns data.
BaseType_t CLI_GetImuData( int8_t *pcWriteBuffer,size_t xWriteBufferLen,const int8_t *pcCommandString )
{
	static axis3bit16_t data_raw_acceleration;
	static axis3bit16_t data_raw_angular_rate;
	static float acceleration_mg[3];
	uint8_t reg;

	lsm6ds3_xl_flag_data_ready_get(GetImuStruct(), &reg);
	if(reg)
	{
		memset(data_raw_acceleration.u8bit, 0x00, 3 * sizeof(int16_t));
		lsm6ds3_acceleration_raw_get(GetImuStruct(), data_raw_acceleration.u8bit);
		acceleration_mg[0] =
		lsm6ds3_from_fs2g_to_mg(data_raw_acceleration.i16bit[0]);
		acceleration_mg[1] =
		lsm6ds3_from_fs2g_to_mg(data_raw_acceleration.i16bit[1]);
		acceleration_mg[2] =
		lsm6ds3_from_fs2g_to_mg(data_raw_acceleration.i16bit[2]);

		snprintf(pcWriteBuffer,xWriteBufferLen, "Acceleration [mg]:X %d\tY %d\t%Z %d\r\n",
		(int)acceleration_mg[0], (int)acceleration_mg[1], (int)acceleration_mg[2]);
	}
	else
	{
		snprintf(pcWriteBuffer,xWriteBufferLen, "No data ready! \r\n");
	}
	return pdFALSE;
}


//THIS COMMAND USES vt100 TERMINAL COMMANDS TO CLEAR THE SCREEN ON A TERMINAL PROGRAM LIKE TERA TERM
//SEE http://www.csie.ntu.edu.tw/~r92094/c++/VT100.html for more info
//CLI SPECIFIC COMMANDS
static char bufCli[CLI_MSG_LEN];
BaseType_t xCliClearTerminalScreen( char *pcWriteBuffer,size_t xWriteBufferLen,const int8_t *pcCommandString )
{
	char clearScreen = ASCII_ESC;
	snprintf(bufCli, CLI_MSG_LEN - 1, "%c[2J", clearScreen);
	snprintf(pcWriteBuffer, xWriteBufferLen, bufCli);
	return pdFALSE;
}


//Example CLI Command. Resets system.
BaseType_t CLI_ResetDevice( int8_t *pcWriteBuffer,size_t xWriteBufferLen,const int8_t *pcCommandString )
{
	system_reset();
	return pdFALSE;
}

/**************************************************************************//**
BaseType_t CLI_NeotrellisSetLed( int8_t *pcWriteBuffer,size_t xWriteBufferLen,const int8_t *pcCommandString )
* @brief	CLI command to turn on a given LED to a given R,G,B, value
* @param[out] *pcWriteBuffer. Buffer we can use to write the CLI command response to! See other CLI examples on how we use this to write back!
* @param[in] xWriteBufferLen. How much we can write into the buffer
* @param[in] *pcCommandString. Buffer that contains the complete input. You will find the additional arguments, if needed. Please see 
https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_CLI/FreeRTOS_Plus_CLI_Implementing_A_Command.html#Example_Of_Using_FreeRTOS_CLIGetParameter
Example 3
                				
* @return		Returns pdFALSE if the CLI command finished.
* @note         Please see https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_CLI/FreeRTOS_Plus_CLI_Accessing_Command_Line_Parameters.html
				for more information on how to use the FreeRTOS CLI.

*****************************************************************************/
BaseType_t CLI_NeotrellisSetLed( int8_t *pcWriteBuffer,size_t xWriteBufferLen,const int8_t *pcCommandString )
{
	static int8_t *pcParameter1, *pcParameter2, *pcParameter3, *pcParameter4;
	static BaseType_t xParameter1StringLength, xParameter2StringLength, xParameter3StringLength, xParameter4StringLength;
	pcParameter1 = FreeRTOS_CLIGetParameter(pcCommandString,1,&xParameter1StringLength);
	pcParameter2 = FreeRTOS_CLIGetParameter(pcCommandString,2,&xParameter2StringLength);
	pcParameter3 = FreeRTOS_CLIGetParameter(pcCommandString,3,&xParameter3StringLength);
	pcParameter4 = FreeRTOS_CLIGetParameter(pcCommandString,4,&xParameter4StringLength);

	/* Terminate both file names. */
	pcParameter1[ xParameter1StringLength ] = 0x00;
	pcParameter2[ xParameter2StringLength ] = 0x00;
	pcParameter3[ xParameter3StringLength ] = 0x00;
	pcParameter4[ xParameter4StringLength ] = 0x00;
	
	uint8_t key_num = atoi(pcParameter1);
	uint8_t red = atoi(pcParameter2);
	uint8_t green = atoi(pcParameter3);
	uint8_t blue = atoi(pcParameter4);
		
	int32_t error = SeesawSetLed(key_num, red, green, blue);
	if (ERROR_NONE != error)
	{
		snprintf(pcWriteBuffer,xWriteBufferLen, "Neo Error!\r\n");
		return pdFALSE;		
	}
	SeesawOrderLedUpdate();
	//Check code SeesawSetLed and SeesawSetLed
	//How do you get parameters? check link in comments!
	//Check that the input is sanitized: Key between 0-15, RGB between 0-255. Print if there is an error!
	//return pdFalse to tell the FreeRTOS CLI your call is done and does not need to call again.
	//This function expects 4 arguments inside pcCommandString: keley, R, G, B.
	snprintf(pcWriteBuffer,xWriteBufferLen, "Success! Key:%u R:%u G:%u B:%u\r\n ", key_num, red, green, blue);
	return pdFALSE;
}

BaseType_t CLI_OLEDdrawCircle( int8_t *pcWriteBuffer,size_t xWriteBufferLen,const int8_t *pcCommandString ){
	int error = NULL;
	static int8_t *pcParameter1, *pcParameter2, *pcParameter3, *pcParameter4;
	static BaseType_t xParameter1StringLength, xParameter2StringLength, xParameter3StringLength, xParameter4StringLength;
	pcParameter1 = FreeRTOS_CLIGetParameter(pcCommandString,1,&xParameter1StringLength);
	pcParameter2 = FreeRTOS_CLIGetParameter(pcCommandString,2,&xParameter2StringLength);
	pcParameter3 = FreeRTOS_CLIGetParameter(pcCommandString,3,&xParameter3StringLength);
	pcParameter4 = FreeRTOS_CLIGetParameter(pcCommandString,4,&xParameter4StringLength);
	
	/* Terminate both file names. */
	pcParameter1[ xParameter1StringLength ] = 0x00;
	pcParameter2[ xParameter2StringLength ] = 0x00;
	pcParameter3[ xParameter3StringLength ] = 0x00;
	pcParameter4[ xParameter4StringLength ] = 0x00;

	
	uint8_t x0 = atoi(pcParameter1);
	uint8_t y0 = atoi(pcParameter2);
	uint8_t radius = atoi(pcParameter3);
	uint8_t color = atoi(pcParameter4);
	uint8_t mode = 0;

	
	MicroOLEDcircle(x0, y0, radius, color, NORM);

	error = MicroOLEDdisplay();
	if (ERROR_NONE != error)
	{
		snprintf(pcWriteBuffer,xWriteBufferLen, "Could not display on OLED!\r\n");
		return pdFALSE;
	}
	snprintf(pcWriteBuffer,xWriteBufferLen, "Circle Outline is drawn!\r\n");
	return pdFALSE;
}