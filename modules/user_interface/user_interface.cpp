//=====[Libraries]=============================================================

#include "mbed.h"
#include "arm_book_lib.h"

#include "user_interface.h"

#include "code.h"
#include "siren.h"
#include "smart_home_system.h"
#include "fire_alarm.h"
#include "date_and_time.h"
#include "temperature_sensor.h"
#include "gas_sensor.h"
#include "matrix_keypad.h"
#include "display.h"
#include "GLCD_fire_alarm.h"
#include "motor.h"
#include "pc_serial_com.h"

//=====[Declaration of private defines]========================================

#define DISPLAY_REFRESH_TIME_REPORT_MS 1000
#define DISPLAY_REFRESH_TIME_ALARM_MS 300

//=====[Declaration of private data types]=====================================

typedef enum{
    DISPLAY_ALARM_STATE,
    DISPLAY_REPORT_STATE
} displayState_t;

//=====[Declaration and initialization of public global objects]===============

DigitalOut incorrectCodeLed(LED3);
DigitalOut systemBlockedLed(LED2);
DigitalOut alarmLed(LED1);

InterruptIn motorDirection1Button(PF_9); //NACHO
InterruptIn motorDirection2Button(PF_8); //NICO



//=====[Declaration of external public global variables]=======================

//=====[Declaration and initialization of public global variables]=============

char codeSequenceFromUserInterface[CODE_NUMBER_OF_KEYS];

//=====[Declaration and initialization of private global variables]============

static displayState_t displayState = DISPLAY_REPORT_STATE;
static int displayAlarmGraphicSequence = 0;
static int displayRefreshTimeMs = DISPLAY_REFRESH_TIME_REPORT_MS;

static bool incorrectCodeState = OFF;
static bool systemBlockedState = OFF;

static bool codeComplete = false;
static int numberOfCodeChars = 0;

//=====[Declarations (prototypes) of private functions]========================

static void userInterfaceMatrixKeypadUpdate();
static void incorrectCodeIndicatorUpdate();
static void systemBlockedIndicatorUpdate();

static void userInterfaceDisplayInit();
static void userInterfaceDisplayUpdate();
static void userInterfaceDisplayReportStateInit();
static void userInterfaceDisplayReportStateUpdate();
static void userInterfaceDisplayAlarmStateInit();
static void userInterfaceDisplayAlarmStateUpdate();

static void motorDirection1ButtonCallback();
static void motorDirection2ButtonCallback();

//=====[Implementations of public functions]===================================

void userInterfaceInit()
{
    motorDirection1Button.mode(PullUp);
    motorDirection2Button.mode(PullUp);

    motorDirection1Button.fall(&motorDirection1ButtonCallback);
    motorDirection2Button.fall(&motorDirection2ButtonCallback);
    
    incorrectCodeLed = OFF;
    systemBlockedLed = OFF;
    matrixKeypadInit( SYSTEM_TIME_INCREMENT_MS );
    userInterfaceDisplayInit();
}

void userInterfaceUpdate()
{
    userInterfaceMatrixKeypadUpdate();
    incorrectCodeIndicatorUpdate();
    systemBlockedIndicatorUpdate();
    userInterfaceDisplayUpdate();
}

bool incorrectCodeStateRead()
{
    return incorrectCodeState;
}

void incorrectCodeStateWrite( bool state )
{
    incorrectCodeState = state;
}

bool systemBlockedStateRead()
{
    return systemBlockedState;
}

void systemBlockedStateWrite( bool state )
{
    systemBlockedState = state;
}

bool userInterfaceCodeCompleteRead()
{
    return codeComplete;
}

void userInterfaceCodeCompleteWrite( bool state )
{
    codeComplete = state;
}

//=====[Implementations of private functions]==================================

static void userInterfaceMatrixKeypadUpdate()
{
    static int numberOfHashKeyReleased = 0;
    char keyReleased = matrixKeypadUpdate();

    if( keyReleased != '\0' ) {

        if( sirenStateRead() && !systemBlockedStateRead() ) {
            if( !incorrectCodeStateRead() ) {
                codeSequenceFromUserInterface[numberOfCodeChars] = keyReleased;
                numberOfCodeChars++;
                if ( numberOfCodeChars >= CODE_NUMBER_OF_KEYS ) {
                    codeComplete = true;
                    numberOfCodeChars = 0;
                }
            } else {
                if( keyReleased == '#' ) {
                    numberOfHashKeyReleased++;
                    if( numberOfHashKeyReleased >= 2 ) {
                        numberOfHashKeyReleased = 0;
                        numberOfCodeChars = 0;
                        codeComplete = false;
                        incorrectCodeState = OFF;
                    }
                }
            }
        }
    }
}

static void userInterfaceDisplayReportStateInit()
{
    displayState = DISPLAY_REPORT_STATE;
    displayRefreshTimeMs = DISPLAY_REFRESH_TIME_REPORT_MS;
    
    displayModeWrite( DISPLAY_MODE_CHAR );

    displayClear();

    displayCharPositionWrite ( 0,0 );
    displayStringWrite( "Temperature:" );

    displayCharPositionWrite ( 0,1 );
    displayStringWrite( "Gas:" );
    
    displayCharPositionWrite ( 0,2 );
    displayStringWrite( "Alarm:" );
}

static void userInterfaceDisplayReportStateUpdate()
{
    char temperatureString[3] = "";
    
    sprintf(temperatureString, "%.0f", temperatureSensorReadCelsius());
    displayCharPositionWrite ( 12,0 );
    displayStringWrite( temperatureString );
    displayCharPositionWrite ( 14,0 );
    displayStringWrite( "'C" );

    displayCharPositionWrite ( 4,1 );

    if ( gasDetectorStateRead() ) {
        displayStringWrite( "Detected    " );
    } else {
        displayStringWrite( "Not Detected" );
    }
    displayCharPositionWrite ( 6,2 );
    displayStringWrite( "OFF" );
}

static void userInterfaceDisplayAlarmStateInit()
{
    displayState = DISPLAY_ALARM_STATE;
    displayRefreshTimeMs = DISPLAY_REFRESH_TIME_ALARM_MS;

    displayClear();

    displayModeWrite( DISPLAY_MODE_GRAPHIC );
   
    displayAlarmGraphicSequence = 0;
}

static void userInterfaceDisplayAlarmStateUpdate()
{
    switch( displayAlarmGraphicSequence ){
        case 0:
            displayBitmapWrite( GLCD_fire_alarm[0] );
            displayAlarmGraphicSequence++;
        break;
        case 1:
            displayBitmapWrite( GLCD_fire_alarm[1] );
            displayAlarmGraphicSequence++;
        break;
        case 2:
            displayBitmapWrite( GLCD_fire_alarm[2] );
            displayAlarmGraphicSequence++;
        break;
        case 3:
            displayBitmapWrite( GLCD_fire_alarm[3] );
            displayAlarmGraphicSequence = 0;
        break;
        default:
            displayBitmapWrite( GLCD_ClearScreen );
            displayAlarmGraphicSequence = 1;
        break;                   
    }
}

static void userInterfaceDisplayInit()
{
    displayInit( DISPLAY_TYPE_GLCD_ST7920, DISPLAY_CONNECTION_SPI );
    userInterfaceDisplayReportStateInit();
}

static void userInterfaceDisplayUpdate()
{
    static int accumulatedDisplayTime = 0;
    
    if( accumulatedDisplayTime >=
        displayRefreshTimeMs ) {

        accumulatedDisplayTime = 0;

        switch ( displayState ) {
            case DISPLAY_REPORT_STATE:
                userInterfaceDisplayReportStateUpdate();

                if ( sirenStateRead() ) {
                    userInterfaceDisplayAlarmStateInit();
                }
            break;

            case DISPLAY_ALARM_STATE:
                userInterfaceDisplayAlarmStateUpdate();

                if ( !sirenStateRead() ) {
                    userInterfaceDisplayReportStateInit();
                }
            break;

            default:
                userInterfaceDisplayReportStateInit();
            break;
        }

   } else {
        accumulatedDisplayTime =
            accumulatedDisplayTime + SYSTEM_TIME_INCREMENT_MS;        
    }
}

static void incorrectCodeIndicatorUpdate()
{
    incorrectCodeLed = incorrectCodeStateRead();
}

static void systemBlockedIndicatorUpdate()
{
    systemBlockedLed = systemBlockedState;
}

static void motorDirection1ButtonCallback()
{
    static int  n; //@note static para que n se mantenga la cuenta
    char string[5];
    
    sprintf(string , "n=%d",n++); //@note paso a string para pasarlo a serial com
    pcSerialComStringWrite(string);

    alarmLed=!alarmLed;
    
    motorDirectionWrite( DIRECTION_1 );
}

static void motorDirection2ButtonCallback()
{
    //motorDirectionWrite( DIRECTION_2 );
    static int j = 0;
    if (j == 0){
        setDutyCycle( RGB_LED_RED, 0.6 );
        setDutyCycle( RGB_LED_GREEN, 0.05 );
        setDutyCycle( RGB_LED_BLUE, 0.05 );
    }
     if (j == 1){
        setDutyCycle( RGB_LED_RED, 0.05 );
        setDutyCycle( RGB_LED_GREEN, 0.6 );
        setDutyCycle( RGB_LED_BLUE, 0.05 );
    }
    if (j == 2){
        setDutyCycle( RGB_LED_RED, 0.005 );
        setDutyCycle( RGB_LED_GREEN, 0.05 );
        setDutyCycle( RGB_LED_BLUE, 0.6 );
    }
    j++;
    
}