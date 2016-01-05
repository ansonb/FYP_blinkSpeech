/**
*
*@author Anson Bastos
*
*
**/
#include <stdint.h>
#include "driverlib.h"
#include "grlib.h"
#include "LcdDriver/Dogs102x64_UC1701.h"
#include "stdio.h"
#include "math.h"
#include <stdint.h>
#include "msp430.h"
#include "HAL_Buttons.h"
#include "HAL_Board.h"
#include "HAL_Dogs102x6.h"
#include "HAL_Wheel.h"
#define ADC_REF 3.7
#define ADC_MAX 4096

#define lcd_clear() GrClearDisplay(&g_sContext);
#define N 100;

#define LEFT 1
#define RIGHT 2
#define UP 3
#define DOWN 4
#define R 32
#define C 51
#define COL_OFFSET 25
#define MAX_MOVE 2000

//286 bytes of ram in use

//global variables

uint16_t timeoutCounter;
tContext g_sContext;
tRectangle g_sRect;
int32_t stringWidth = 0;

//Back end variables
short isCharReceived = 0;
char receivedChar;
float  AdcThreshold=255,temp=0;
float  FThreshold,tempF=0;
float  X;
float  dcval=0;
int    fourier_offset=0;
unsigned int eeprom_address;
short  input;     //on pin 6.6
short  ON=0,calibrated_adc=0,calibrated_steady=0,calibrated_F=0,menu_counter;
short  i=0;
short  small_blink=0,big_blink=0,steady_trough=255,steady_crest=0,trough=0,crest=0,max_input=0,view_counter=0;
short  temp_BigB=0,counter_BigB=0;

//UI variables
int menu=0,option=0,exec_counter=0;
int max_menu=7;
int total_options[]={6,4,5,6,2,0,0};
int disp_counter=1;

//Snake variables
uint8_t head_r=0, head_c=0, tail_r=0, tail_c=0;
uint8_t facing = RIGHT;
uint16_t mat[R][C];
uint16_t no_of_turns = 0;
uint16_t move = 0;
uint16_t length = 0;

//function declarations
void Clock_init(void);
void Delay(void);
void switch_init();
void lcd_init();
void adc_init();
int adc_read();
void _delay_us(int us);
void _delay_ms(int ms);
void _delay_s(int s);
void eeprom_write_bytes(char* bytes, unsigned int num_bytes);
char eeprom_read_byte(unsigned int address);
void set_up();
void bot_StartStop();
float dft();
void check_blink();
void check_fourier_offset();
void eye_control();
void usart_init();
short usart_receive_char(char*);
void usart_transmit_char(char);
void usart_tansmit_string(char*);
void usart_transmit_int_as_string(int);


//function definitions
void usart_init()
{
    P4SEL = BIT4+BIT5;
    UCA1CTL1 |= UCSWRST;                      // **Put state machine in reset**
    UCA1CTL1 |= UCSSEL_1;                     // CLK = ACLK
    UCA1BR0 = 0x03;                           // 32kHz/9600=3.41 (see User's Guide)
    UCA1BR1 = 0x00;                           //
    UCA1MCTL = UCBRS_3+UCBRF_0;               // Modulation UCBRSx=3, UCBRFx=0
    UCA1CTL1 &= ~UCSWRST;                     // **Initialize USCI state machine**
    UCA1IE |= UCRXIE;                         // Enable USCI_A1 RX interrupt
    isCharReceived = 0;
    __enable_interrupt();
}

short usart_receive_char(char* ch)
{
    if(isCharReceived)
    {
        *ch = receivedChar;
        isCharReceived = 0;
        return 1;
    }
    else return 0;
}

void usart_transmit_char(char ch)
{
    while(!(UCA1IFG & UCTXIFG));              // Wait until transmit buffer is empty
    UCA1TXBUF = ch;
}

void usart_transmit_string(char* str)
{
    while(*str)
    {
        usart_transmit_char(*str);
        ++ str;
    }
}

void usart_transmit_int_as_string(int i)
{
    char num_string[6];
    sprintf(num_string,"%d",i);
    usart_transmit_string(num_string);
}

//Check Compiler and write syntax of ISR function
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=USCI_A1_VECTOR
__interrupt void USCI_A1_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(USCI_A1_VECTOR))) USCI_A1_ISR (void)
#else
#error Compiler not supported!
#endif
//Interrupt Service Routine
{
  switch(__even_in_range(UCA1IV,4))
  {
  case 0:break;                             // Vector 0 - no interrupt
  case 2:                                   // Vector 2 - RXIFG
      isCharReceived = 1;
      receivedChar = UCA1RXBUF;
      break;
  case 4:break;                             // Vector 4 - TXIFG
  default: break;
  }
}

void reinitialise(uint8_t mat[][C])
{
	GrClearDisplay(&g_sContext);
	GrStringDrawCentered(&g_sContext, "Snake", 5, 51, 32, 0);
	_delay_ms(1000);
	GrClearDisplay(&g_sContext);

	int i,j;
	head_r=0, head_c=0, tail_r=0, tail_c=0, no_of_turns=0;
	move = 0;
	facing = RIGHT;
	length = 1;

	for(i=0; i<R; i++)
	{
		for(j=0; j<C; j++)
		{
			mat[i][j] = 0;
		}
	}
	mat[0][0] = 1;
}

uint8_t moveSnake(uint8_t dir)
{
	switch(facing)
	{
	case RIGHT:
		switch(dir)
		{
		case LEFT:
			head_r=(R+head_r-1)%R;
			facing=UP;
			break;
		case RIGHT:
			head_r=(head_r+1)%R;
			facing=DOWN;
			break;
		case UP:
			head_c=(head_c+1)%C;
			facing=RIGHT;
			break;
		}
		break;

	case LEFT:
		switch(dir)
		{
		case LEFT:
			head_r=(head_r+1)%R;
			facing=DOWN;
			break;
		case RIGHT:
			head_r=(R+head_r-1)%R;
			facing=UP;
			break;
		case UP:
			head_c=(C+head_c-1)%C;
			facing=LEFT;
			break;
		}
		break;

	case UP:
		switch(dir)
		{
		case LEFT:
			head_c=(C+head_c-1)%C;
			facing=LEFT;
			break;
		case RIGHT:
			head_c=(head_c+1)%C;
			facing=RIGHT;
			break;
		case UP:
			head_r=(R+head_r-1)%R;
			facing=UP;
			break;
		}
		break;

	case DOWN:
		switch(dir)
		{
		case LEFT:
			head_c=(head_c+1)%C;
			facing=RIGHT;
			break;
		case RIGHT:
			head_c=(C+head_c-1)%C;
			facing=LEFT;
			break;
		case UP:
			head_r=(head_r+1)%R;
			facing=DOWN;
			break;
		}
		break;

	default:
		break;
	}

	if(mat[head_r][head_c]!=0)
	{
		return 0;
	}
	if((head_r+head_c+2)%2 == 1)
	{
		mat[head_r][head_c]=move;
		length++;

		Dogs102x64_UC1701PixelDraw(0, head_c + COL_OFFSET, head_r, 0);
	}
	else
	{
		uint16_t temp = mat[tail_r][tail_c];

		mat[head_r][head_c]=move;
		mat[tail_r][tail_c]=0;

		Dogs102x64_UC1701PixelDraw(0, head_c + COL_OFFSET, head_r, 0);
		Dogs102x64_UC1701PixelDraw(0, tail_c + COL_OFFSET, tail_r, 1);
		//GrPixelDraw(&g_sContext, head_r, head_c);

		if(mat[(R+tail_r-1)%R][tail_c]==(temp+1)) tail_r=(R+tail_r-1)%R;
		else if(mat[(tail_r+1)%R][tail_c]==(temp+1)) tail_r=(tail_r+1)%R;
		else if(mat[tail_r][(C+tail_c-1)%C]==(temp+1)) tail_c=(C+tail_c-1)%C;
		else if(mat[tail_r][(tail_c+1)%C]==(temp+1)) tail_c=(tail_c+1)%C;
	}


	return 1;
}

void playSnake(){
	buttonsPressed = 0;
	uint8_t i,j,play_again=0;
	uint8_t dir = UP;

	GrStringDrawCentered(&g_sContext, "Snake", 5, 51, 32, 0);
	_delay_ms(1000);
	GrClearDisplay(&g_sContext);


	for(i=0; i<R; i++){
		for(j=0; j<C; j++){
			mat[i][j] = 0;
		}
	}
	mat[0][0] = 1;

	while(1){
		move = (move+1)%MAX_MOVE;
		_delay_ms(100);
		if (buttonsPressed & BUTTON_S1){
			buttonsPressed = 0;
			dir = RIGHT;
			no_of_turns++;
		}
		else if (buttonsPressed & BUTTON_S2){
			buttonsPressed = 0;
			dir = LEFT;
			no_of_turns++;
		}
		else{
			dir = UP;
		}

		if(moveSnake(dir))
		{
			continue;
		}
		else
		{
			print_string(2, 37, "Length     : ");
			print_int(78, 37, length);
			print_string(2, 45, "No of Turns: ");
			print_int(78, 45, no_of_turns);
			_delay_ms(100);

			print_string(2, 53, "S1=Play S2=Quit");
			while(1)
			{
				if(buttonsPressed & BUTTON_S1)
				{
					buttonsPressed = 0;
					play_again=1;
					break;
				}
				if (buttonsPressed & BUTTON_S2)
				{
					buttonsPressed = 0;
					play_again=0;
					break;
				}
			}
			if(play_again)
			{
				reinitialise(mat);
				play_again=0;
				continue;
			}
			else
			{
				break;
			}
		}

	}

	lcd_clear();
}

void Clock_init(void)
{
	UCS_setExternalClockSource(
                               32768,
                               0);

    // Set Vcore to accomodate for max. allowed system speed
    PMM_setVCore(
    		PMM_CORE_LEVEL_3
                );

    // Use 32.768kHz XTAL as reference
    UCS_LFXT1Start(
        UCS_XT1_DRIVE0,
        UCS_XCAP_3
        );


    // Set system clock to max (25MHz)
    UCS_initFLLSettle(
    	25000,
        762
        );

    SFR_enableInterrupt(
                        SFR_OSCILLATOR_FAULT_INTERRUPT
                       );

    // Globally enable interrupts
    __enable_interrupt();
}


void Delay(void)
{
	__delay_cycles(SYSTEM_CLOCK_SPEED );
}

void switch_init(){
	 __enable_interrupt();

	 Buttons_init(BUTTON_S1 );
	 Buttons_interruptEnable(BUTTON_S1 );

	 Buttons_init(BUTTON_S2 );
	 Buttons_interruptEnable(BUTTON_S2 );

	 buttonsPressed = 0;
}

void lcd_init(){
	Dogs102x64_UC1701Init();
	GrContextInit(&g_sContext, &g_sDogs102x64_UC1701);
	GrContextForegroundSet(&g_sContext, ClrBlack);
	GrContextBackgroundSet(&g_sContext, ClrWhite);
	GrContextFontSet(&g_sContext, &g_sFontFixed6x8);
	GrClearDisplay(&g_sContext);
}

void adc_init()
{
    ADC12CTL0 = ADC12SHT0_9|ADC12ON;       // Sampling time, ADC12 on
    ADC12CTL1 = ADC12SHP;                  // Use sampling timer
    ADC12MCTL0 = ADC12INCH_6;
    ADC12CTL0 |= ADC12ENC;
    P6SEL |= BIT6 ;                        // P6.6 ADC option select
    ADC12CTL0 &= ~ADC12SC;                 // Clear the ADC start bit
}

int adc_read()
{
    ADC12CTL0 |= ADC12SC + ADC12ENC;       // Start sampling/conversion
    while (ADC12CTL1 & ADC12BUSY);         // Wait until conversion is complete
    return (ADC12MEM0 & 0x0FFF)/16;             // Mask 4 upper bits of ADC12MEM0(12 bit ADC); div by 16 for 8 bit adc
}

void _delay_us(int us)
{
    //while(us --) __delay_cycles(1);
	while(us --) __delay_cycles(SYSTEM_CLOCK_SPEED/1000000);
}

void _delay_ms(int ms)
{
    //while(ms --) __delay_cycles(1087);
	while(ms --) __delay_cycles(SYSTEM_CLOCK_SPEED/1000);
}

void _delay_s(int s)
{
    //while(s --) __delay_cycles(1086957);
	while(s --) __delay_cycles(SYSTEM_CLOCK_SPEED);
}

void eeprom_write_bytes(char* bytes, unsigned int num_bytes)
{
  char *Flash_ptr;                          // Initialize Flash pointer
  Flash_ptr = (char *) 0x1880;
  __disable_interrupt();
  FCTL3 = FWKEY;                            // Clear Lock bit
  FCTL1 = FWKEY+ERASE;                      // Set Erase bit
  *Flash_ptr = 0;                           // Dummy write to erase Flash seg
  FCTL1 = FWKEY+WRT;                        // Set WRT bit for write operation
  unsigned int i;
  for(i = 0 ; i < num_bytes ; ++ i)
      *Flash_ptr++ = bytes[i];              // Write value to flash
  FCTL1 = FWKEY;                            // Clear WRT bit
  FCTL3 = FWKEY+LOCK;                       // Set LOCK bit
  __enable_interrupt();
}

char eeprom_read_byte(unsigned int address)
{
  char *Flash_ptr;                          // Initialize Flash pointer
  Flash_ptr = (char *) 0x1880;
  FCTL3 = FWKEY;
  char byte = Flash_ptr[address];
  FCTL3 = FWKEY + LOCK;
  return byte;
}

void set_up(){
     // Stop WDT
     WDT_A_hold(WDT_A_BASE);

     // Basic GPIO initialization
     Board_init();
     Clock_init();

     //Enable Bluetooth communication via usart
     usart_init();

     //Enable Wheel
     Wheel_init();

     // Set up LCD
     lcd_init();

     print_string(22,30,"*WELCOME*");
     _delay_ms(2000);

     //Buttons S1 S2
	 switch_init();

	 //PORTD=0xFF;
	 //adc 6.6 pin
	 adc_init();
	 //DDRC=0XFF;

	 eeprom_address=0;
	 AdcThreshold=eeprom_read_byte(eeprom_address);
	 eeprom_address++;
	 dcval=eeprom_read_byte(eeprom_address);
	 eeprom_address++;
	 steady_crest=eeprom_read_byte(eeprom_address);
	 eeprom_address++;
	 fourier_offset=eeprom_read_byte(eeprom_address);
	 eeprom_address++;
	 FThreshold=eeprom_read_byte(eeprom_address);

	 //usart_init();
	 //pwm_init();

}

void bot_StartStop(){
     if(ON){
		 lcd_clear();
		 print_string(50,30,"stop");
		 bot_stop();
		 _delay_ms(1000);
		 lcd_clear();
		 ON=0;
	 }
	  else if(!ON){
		 lcd_clear();
		 print_string(50,30,"start");
		 _delay_ms(1500);     //will move forward for minimum 500 ms till told to stop
		 bot_forward();
		 lcd_clear();
		 ON=1;
	 }
}

float dft(){
     int Ns=100;
     int n,k,data[100];
     float ReX,ImX,Xl;

	 for(k=0;k<Ns;k++){ //sample data with frequency Fs
		 data[k] = adc_read();    // read the input pin
		 _delay_us(900);    //Ts=10e5/N usec
	 }

	 k=1;
	 ReX=0;
	 ImX=0;
	 for(n=0;n<Ns;n++){               //DFT
		 ReX=ReX+cos(2*3.14*n*k/Ns)*data[n]*0.01;
		 ImX=ImX+sin(2*3.14*n*k/Ns)*data[n]*0.01;
	 }
         //this is only to compute the max sample value;remove if not needed
		 temp_BigB=0;
		 counter_BigB=0;
         for(n=0;n<100;n++){
                 if(max_input<data[n]) max_input=data[n];
				 /*if(data[n]>70) {//when noise level of big blink remains above this level
				     temp_BigB++;
					 if(temp_BigB>counter_BigB)//store max count
					     counter_BigB=temp_BigB;//store before reset
				 }
				 else temp_BigB=0;//reset as normally spikes go above and below*/
         }

         //compute the magnitude
	 Xl=(ReX*ReX+ImX*ImX);

	 return Xl;

}

void check_blink(){
         max_input=0;
         X=dft();
         if(X>FThreshold){
             if(max_input>86)  big_blink=1; //see the max adc val corresssponding to saturation output of eog ckt
		     //if(X>400)  big_blink=1;//big blink fourier value;same values for small and big blink
		     //if(counter_BigB>7) big_blink=1;//arbitrary counter
		     else
                 small_blink=1;
         }

}

void check_fourier_offset(){
        X=dft();
        if(X>fourier_offset){ //typecast if needed
                fourier_offset=X;
        }
}


void highlight_and_print_string(int column,int row,char *s){
    GrContextForegroundSet(&g_sContext, ClrWhite);
    GrContextBackgroundSet(&g_sContext, ClrBlack);

    GrStringDraw(&g_sContext,
    		s,
    		AUTO_STRING_LENGTH,
    		column,
    		row,OPAQUE_TEXT);
}

void print_char(int column,int row,const char s){
    GrContextForegroundSet(&g_sContext, ClrBlack);
    GrContextBackgroundSet(&g_sContext, ClrWhite);

    char S[2] = {s, 0};
    GrStringDraw(&g_sContext,
    		S,
    		AUTO_STRING_LENGTH,
    		column,
    		row,OPAQUE_TEXT);
}

void print_string(int column,int row,char *s){
    GrContextForegroundSet(&g_sContext, ClrBlack);
    GrContextBackgroundSet(&g_sContext, ClrWhite);

    GrStringDraw(&g_sContext,
    		s,
    		AUTO_STRING_LENGTH,
    		column,
    		row,OPAQUE_TEXT);
}

void print_int(int column,int row,int n){
    GrContextForegroundSet(&g_sContext, ClrBlack);
    GrContextBackgroundSet(&g_sContext, ClrWhite);

    char s[10];
    snprintf(s,10,"%d",n);

    GrStringDraw(&g_sContext,
    		s,
    		AUTO_STRING_LENGTH,
    		column,
    		row,TRANSPARENT_TEXT);
}




void plot(){
	unsigned int sample1,sample2,i=0;

	 Buttons_init(BUTTON_S2);
	 Buttons_interruptEnable(BUTTON_S2);


	 Buttons_init(BUTTON_S1);
	 Buttons_interruptEnable(BUTTON_S1);

     buttonsPressed = 0;

    Dogs102x6_init();
    Dogs102x6_backlightInit();
    Dogs102x6_setBacklight(11);
    Dogs102x6_setContrast(11);
    Dogs102x6_clearScreen();

    int buffer[104];
    int j;
    for(j=0; j<102; j++){
        buffer[j] = 0;
    }

    Dogs102x6_stringDraw(7, 0, "S2=Esc S1=P/R", DOGS102x6_DRAW_NORMAL);
    while(1)
    {
    	if(buttonsPressed & BUTTON_S2){
    		  _delay_ms(200);
    	      buttonsPressed=0;
    	      lcd_init(); //initialise grlib
    	      break;
    	 }
          if(i == 101)
          {
              Dogs102x6_clearScreen();
              int j;
              for(j=0; j<102; j++){
            	  buffer[j] = 0;
              }
              Dogs102x6_stringDraw(7, 0, "S2=Esc S1=P/R",DOGS102x6_DRAW_NORMAL);
              i = 0;
           }
           ADC12CTL0 |= ADC12SC + ADC12ENC;
           // Start sampling/conversion
           while (ADC12CTL1 & ADC12BUSY) __no_operation();
           sample1 = ADC12MEM0 & 0x0FFF;
           buffer[i]=sample1;
           __delay_cycles(1000000);
           ADC12CTL0 |= ADC12SC + ADC12ENC;
           // Start sampling/conversion
           while (ADC12CTL1 & ADC12BUSY) __no_operation();
           sample2 = ADC12MEM0 & 0x0FFF;
           buffer[i+1]=sample2;
           Dogs102x6_lineDraw(i,56-sample1/86,i+1,56-sample2/86,0);

           //Pause the screen
           if(buttonsPressed & BUTTON_S1){
        	   _delay_ms(200);
               buttonsPressed = 0;

               while(!(buttonsPressed & BUTTON_S1)){
            	   Dogs102x6_clearScreen();
            	   int j;
            	   for(j=0; j<104; j++){
            	       Dogs102x6_lineDraw(j,56-buffer[j]/86,j+1,56-buffer[j+1]/86,0);
            	   }
            	   Wheel_init();
            	   int pos = Wheel_getValue();
            	   Dogs102x6_lineDraw(0,(int)(pos*48)/4096+8,104,(int)(pos*48)/4096+8,0);

            	   char c1[10], c2[10];
            	   int reverse_adc = 86*(48 - (pos*48)/4096);
            	   snprintf(c1, 10,"%d", reverse_adc);
            	   Dogs102x6_stringDraw(56, 0, c1, DOGS102x6_DRAW_NORMAL);
            	   int voltage = reverse_adc*3.7/4096.0 * 1000; //Voltage in mV
            	   snprintf(c2, 10,"%d", voltage);
            	   Dogs102x6_stringDraw(0, 0, c2, DOGS102x6_DRAW_NORMAL);
            	   Dogs102x6_stringDraw(0, 28, "mV", DOGS102x6_DRAW_NORMAL);

            	   _delay_ms(500);
               }
               _delay_ms(200);
               buttonsPressed = 0;
               Dogs102x6_clearScreen();
               adc_init(); //init required to use 6.6
           }

           ++ i;


      }
}

void eye_control(){
	print_string(0,0,"***Eye Control***");
	print_string(0,56,"*S2=Exit*");
    while(1){
    	//S2 to disable eye control
		 if(buttonsPressed & BUTTON_S2){
			 _delay_ms(200);
			 buttonsPressed=0;
			 lcd_clear();
			 return;
		 }

        small_blink=0, big_blink=0;
		input = adc_read();

		 if(input > AdcThreshold || input==AdcThreshold){
		    check_blink();
		 }

		if(small_blink){
			usart_transmit_char('b');
			print_string(10,40,"Transnitting b");
			_delay_ms(200);
			lcd_clear();
			print_string(0,0,"***Eye Control***");
			print_string(0,56,"*S2=Exit*");
		}

		else if(big_blink){
			usart_transmit_char('B');
			print_string(8,28,"Transmitting B");
			_delay_ms(200);
			lcd_clear();
			print_string(0,0,"***Eye Control***");
			print_string(0,56,"*S2=Exit*");
		}
	 }
}

void main(void)
{
	set_up();

	    buttonsPressed = 0;
	    while(1){

	    	exec_counter=0;
	    	//to select menu

		    //store only when any value has changed
			if(calibrated_adc==1||calibrated_F==1||calibrated_steady==1){
	             char storage[]={AdcThreshold, dcval, steady_crest, fourier_offset, FThreshold};
					 eeprom_write_bytes(storage,5);//check for FThreshold>255

		 /*   for(i=0;i<4;i++){
				P1OUT = 0b00000001;
		        _delay_ms(250);
		        P1OUT = 0b00000000;
		        _delay_ms(250);
		    }
	*/
				 }


			tempF=255;
			temp=0;
			//to refresh the  thresholds only when switches are pressed
			calibrated_adc=0;
	        calibrated_F=0;
			calibrated_steady=0;

	    	if (buttonsPressed & BUTTON_S2){
	    		_delay_ms(200);
	    		buttonsPressed = 0;
	    		if(menu==0) {
	    			menu=option+1;
	    			option=0;
	    		}
	    		else exec_counter=1;
	    		disp_counter=1;

	    	}

	    	//to scroll options
	    	if (buttonsPressed & BUTTON_S1){
	    		_delay_ms(200);
	    		buttonsPressed = 0;
	    		if(option<(total_options[menu]-1)){
	    			option++;
	    		}
	    		else option=0;
	    		disp_counter=1;
	        }

	        if(disp_counter==1){
	        	disp_counter=0;//to prevent  writing if no change
	        	GrClearDisplay(&g_sContext);
	        	switch(menu){
	        	case 0:
	        		print_string(0,0,"=====Menu====");
	        		print_string(0,8,"1.Calibrate");
	        		print_string(0,16,"2.View Thresholds");
	        		print_string(0,24,"3.Enable Eye ");
	        		print_string(0,32,"4.View Signal");
	        		print_string(0,40,"5.Test Bluetooth");
	        		print_string(0,48,"6.Switch Control");
	        		switch(option){
	        		case 1:
	        			highlight_and_print_string(0,16,"2.View Thresholds");
	        			break;

	        		case 2:
	        			highlight_and_print_string(0,24,"3.Enable Eye ");
	        			break;

	        		case 3:
	        			highlight_and_print_string(0,32,"4.View Signal");
	        			break;

	        		case 4:
	        			highlight_and_print_string(0,40,"5.Test Bluetooth");
	        			break;

	        		case 5:
	        			 highlight_and_print_string(0,48,"6.Switch Control");
	        			 break;

	        		default:
	        			highlight_and_print_string(0,8,"1.Calibrate");
	        			option=0;
	        			break;
	        		}
	        		break;

	        	case 1:
	        		print_string(0,0,"===Calibrate===");
	        		print_string(0,8,"1.Steady Values");
	        		print_string(0,16,"2.Peak");
	        		print_string(0,24,"3.Fourier ");
	        		print_string(0,32,"4.Back");


					tempF=255;
					temp=0;
					//to refresh the  thresholds only when switches are pressed
					calibrated_adc=0;
	                calibrated_F=0;
					calibrated_steady=0;

	        		switch(option){
	        		case 1:
	        			highlight_and_print_string(0,16,"2.Peak");
	        			if(exec_counter==1){
	        				exec_counter=0;
	        				lcd_clear();
	        				 //to calibrate small blink adc Thresholds;cannot view adc values till out
	        				while(!(buttonsPressed & BUTTON_S2)){
	        					if(calibrated_adc==0) {
	        					    AdcThreshold=255;
	        						lcd_clear();
	        						print_string(0,0,"Blink to calib");
	        						print_int(50, 30, AdcThreshold);
	        						print_string(0,56,"*S2=ESC*");
	                                _delay_ms(200);
	        					}
	        					temp = adc_read();
	        					_delay_ms(20);
	        					crest=0;
	        					// to check crest
	        					while(temp<adc_read()){
	        					temp = adc_read();
	        					_delay_ms(20);
	        					crest=1;
	        				   }
	        				   if(crest && temp>steady_crest && temp<AdcThreshold){
	        					  AdcThreshold=temp;
	        				   }
	        				   calibrated_adc=1;
	        				   lcd_clear();
							   // print_string(0,0,"Blink to calibrate");
							   //print_string(56,0,"*S2=ESC*");
	        				   print_string(0,0,"Blink to calib");
	        				   print_int(50, 30, AdcThreshold);
	        				   print_string(0,56,"*S2=ESC*");
	        				   _delay_ms(200);
	        		     }

	        				if(AdcThreshold==255) AdcThreshold=steady_crest;//if 50 hz noise
	        				lcd_clear();

	        				_delay_ms(200);
	        			    buttonsPressed=0;
	        				disp_counter=1;

	        			}
	        			break;

	        		case 2:
	        			highlight_and_print_string(0,24,"3.Fourier");
	        			if(exec_counter==1){
	        				exec_counter=0;

	       				 //calibrate FThreshold
	       				 while(!(buttonsPressed & BUTTON_S2)){
	       				 if(calibrated_F==0){
	       					//write anything that you want to execute only once in the loop
	       					 lcd_clear();

	       				 }
	       				 X=dft();
	       				 if(X>fourier_offset+10 && tempF>X){  //+10 is for lesser sensitivity. If offset is more use % of fourier_offset
	       					 FThreshold=X;
	       					 tempF=X;
	       				 }
	       				 calibrated_F=1;
	       				 lcd_clear();
	   					 print_string(0,0,"Blink to calibrate");
	   					 print_string(0,56,"*S2=ESC*");
	       				 print_int(50,30,(int)X);
	       				 _delay_ms(200);
	       				 }
	       				    lcd_clear();
	       				    _delay_ms(200);
	        				buttonsPressed=0;
	        				disp_counter=1;
	        			}
	        			break;

	        		case 3:
	        			highlight_and_print_string(0,32,"4.Back");
	        			if(exec_counter==1){
	        				exec_counter=0;
	        				menu=0;
	        				option=0;
	        				disp_counter=1;
	        			}
	        			break;

	        		default:
	        			highlight_and_print_string(0,8,"1.Steady Values");
	        			option=0;
	        			if(exec_counter==1){
	        				exec_counter=0;
	   				     //initialize before setting
	   					 dcval=0;
	   					 fourier_offset=0;
	   					 steady_trough=255,steady_crest=0;

	   					 lcd_clear();
	   					 print_string(0,0,"Focus on the dot");
	   					 print_string(32,16,"for 6s ");
	   					 print_string(40,30,"(.)");
	   					 _delay_s(3);

	   					 //compute fourier_offset
	   					 for(i=0;i<10;i++){  //will check for 10*100 ms
	   					  check_fourier_offset();
	   					 }
	   					 //compute dc_val
	   					 for(i=0;i<2000;i++){ //average 2000 samples collected over 2 sec
	   					 input=adc_read();
	   					  dcval+=(float)input/2000;
	   					  if(steady_crest<input) steady_crest=input;
	   					  if(steady_trough>input) steady_trough=input;
	   					  _delay_ms(1);
	   					 }
	   					 steady_crest=steady_crest+(steady_crest-dcval)*0;  //0% more than the steady peak
	   					 steady_trough=steady_trough-(dcval-steady_trough)*0;

	   					 lcd_clear();
	   					 print_string(0,0,"Done Calibrating!");
	   					 print_string(0,16,"Fourier Off:");
	   					 print_int(80,16,fourier_offset);
	   					 print_string(0,32,"DC:");
	   					 print_int(80,32,dcval);
	   					 print_string(0,40,"Steady Crest:");
	   					 print_int(80,40,steady_crest);
	   					 print_string(0,56,"*S2=ESC*");
	   					 _delay_s(3);
	   					 while(!(buttonsPressed & BUTTON_S2));
	   					 lcd_clear();

	   					 calibrated_steady=1;

	   					 //to display when it goes back
	   					 _delay_ms(200);
						 buttonsPressed=0;
						 disp_counter=1;
	        			}
	        			break;
	        		}
	        		break;

	        	case 2:
	        		print_string(0,0,"===Thresholds===");
	        		print_string(0,8,"1.DC.........");
	        		print_int(80,8,dcval);
	        		print_string(0,16,"2.Steady Pk..");
	        		print_int(80,16,steady_crest);
	        		print_string(0,24,"3.Peak.......");
	        		print_int(80,24,AdcThreshold);
	        		print_string(0,32,"4.Fourier....");
	        		print_int(80,32,FThreshold);
	        		print_string(0,40,"4.F offset...");
	        		print_int(80,40,fourier_offset);
	        		print_string(0,56,"*S2=ESC*");
	                while(!(buttonsPressed & BUTTON_S2));
	                _delay_ms(200);
	                buttonsPressed=0;
					menu=0;
					option=0;
					disp_counter=1;
	        		break;
	        	case 3:
	    			eye_control();

					menu=0;
					option=0;
					disp_counter=1;

	        		break;

	        	//debug signal on lcd
	        	case 4:
	        		print_string(0,0,"S1 pause/resume");
	        		print_string(0,56,"S2 Back");
	        		plot();
	    		    menu=0;
	    		    option=0;
	    		    disp_counter=1;
	        		break;

	        	case 5:
	        	    print_string(10,10,"Bluetooth Test");
	        	    print_string(0,56,"*S2 Exit*");
	        	    _delay_ms(1000);
	        	    char ch;
	        	    while(1)
	        	    {
	        	    	//Exit on S2
	        	    	if(buttonsPressed & BUTTON_S2){
	        	    		_delay_ms(200);
	        	    		buttonsPressed = 0;
	        	    		menu=0;
	        	    		option=0;
	        	    		disp_counter=1;
	        	    		break;
	        	    	}

	        	    	if(buttonsPressed & BUTTON_S1){
	        	    		_delay_ms(200);
	        	    		buttonsPressed = 0;
	        	    		lcd_clear();
	        	    		_delay_ms(200);
	        	    		playSnake();
	        	    		print_string(10,10,"Bluetooth Test");
	        	    		print_string(0,56,"*S2 Exit*");
	        	    		_delay_ms(200);
	        	    	}

	        	        if(usart_receive_char(&ch)){
	        	        	lcd_clear();
	        	            print_char(50,30,ch);
	        	            print_string(10,10,"Bluetooth Test");
	        	            print_string(0,56,"*S2 Exit*");
	        	            _delay_ms(100);
	        	            usart_transmit_char(ch);
	        	        }
	        	    }
	        	    break;

	        	case 6:
	        		print_string(0,0,"===Switch Control===");
	        		print_string(0,8,"1.Transmit b");
	        		print_string(0,16,"2.Transmit B");
	        		print_string(0,48,"Wheel=Scroll");
	        		print_string(0,56,"S2=Back S1=Sel");

	        		highlight_and_print_string(0,8,"1.Transmit b");

	        		char c = 'b';
	        		int pos=0;
	        		int prev_pos=0;

	        		while(1){
	        			//Exit on S2
	        			if(buttonsPressed & BUTTON_S2){
	        				_delay_ms(200);
	        				buttonsPressed = 0;
	        				break;
	        			}

	        			//Select option if S1
	        			if(buttonsPressed & BUTTON_S1){
	        				_delay_ms(200);
	        				buttonsPressed = 0;
	        				usart_transmit_char(c);
	        			}

	        			Wheel_init();
	        			pos = Wheel_getPosition();

	        			if(prev_pos != pos){
	        				lcd_clear();
	    	        		print_string(0,0,"===Switch Control===");
	    	        		print_string(0,8,"1.Transmit b");
	    	        		print_string(0,16,"2.Transmit B");
	    	        		print_string(0,48,"Wheel=Scroll");
	    	        		print_string(0,56,"S2=Back S1=Sel");

		        			if(pos%2==0){
		        				c = 'b';
		    	        		highlight_and_print_string(0,8,"1.Transmit b");
		        			}else{
		        				c = 'B';
		    	        		highlight_and_print_string(0,16,"2.Transmit B");
		        			}

		        			_delay_ms(200);
	        			}

	        			prev_pos = pos;
	        		}

	        		menu=0;
	        		option=0;
	        		disp_counter=1;
	        		break;

	        	default:break;
	        	}
	        }
	    }

}

