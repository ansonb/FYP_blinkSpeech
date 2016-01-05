/* --COPYRIGHT--,BSD
 * Copyright (c) 2013, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * --/COPYRIGHT--*/
#include <msp430f5529.h>
#include "grlib.h"
#include "driverlib.h"
#include "Dogs102x64_UC1701.h"


uint8_t DOGS102x64Memory[816] = {0};
uint8_t ucBacklightLevel = 11;
uint8_t ucContrast = 11;

//*****************************************************************************
//
// Writes a data word to the UC1701.  This function implements the basic SPI
// interface to the LCD display.
//
//*****************************************************************************
static void
WriteData(uint16_t usData)
{  
    // Store current GIE state
    uint16_t gie = __get_SR_register() & GIE;

    // Make this operation atomic
    __disable_interrupt();

    ClearCS();

    GPIO_setOutputHighOnPin(LCD_CD_PORT,LCD_CD_PIN);
    
    // USCI_B1 TX buffer ready?
    while(!USCI_B_SPI_getInterruptStatus(USCI_B1_BASE,
                                  USCI_B_SPI_TRANSMIT_INTERRUPT
                                  ));
    
    // Transmit data and increment pointer
    USCI_B_SPI_transmitData(USCI_B1_BASE,
                     usData
                     );

    // Wait for all TX/RX to finish
    while(USCI_B_SPI_isBusy(USCI_B1_BASE));
    
    // Dummy read to empty RX buffer and clear any overrun conditions
    USCI_B_SPI_receiveData(USCI_B1_BASE);
    
    // CS High
    SetCS();
    
    // Restore original GIE state
    __bis_SR_register(gie);
}

//*****************************************************************************
//
// Writes a command to the UC1701.  This function implements the basic SPI
// interface to the LCD display.
//
//*****************************************************************************
static void
WriteCommand(uint8_t ucData)
{
    // Store current GIE state
    uint16_t gie = __get_SR_register() & GIE;

    // Make this operation atomic
    __disable_interrupt();

    // CS Low
    ClearCS();

    // CD Low
    GPIO_setOutputLowOnPin(LCD_CD_PORT,LCD_CD_PIN);
    
    // USCI_B1 TX buffer ready?
    while(USCI_B_SPI_TRANSMIT_INTERRUPT != USCI_B_SPI_getInterruptStatus(
    										  USCI_B1_BASE,
                                              USCI_B_SPI_TRANSMIT_INTERRUPT));
    // Transmit data
    USCI_B_SPI_transmitData(
       USCI_B1_BASE,
       ucData
       );
    
    // Wait for all TX/RX to finish
    while (USCI_B_SPI_BUSY == USCI_B_SPI_isBusy(USCI_B1_BASE)) ;
     
    // Dummy read to empty RX buffer and clear any overrun conditions
    USCI_B_SPI_receiveData(USCI_B1_BASE);
       
    // CS High
    SetCS();

    // Restore original GIE state
    __bis_SR_register(gie);
}

//*****************************************************************************
//
// Initializes the pins required for the GPIO-based LCD interface.
//
// This function configures the GPIO pins used to control the LCD display
// when the basic GPIO interface is in use. On exit, the LCD controller
// has been reset and is ready to receive command and data writes.
//
// \return None.
//
//*****************************************************************************
static void
InitGPIOLCDInterface(void)
{
    //
    // Configure the pins that connect to the LCD as outputs.
    //
    
    // Configure LCD_RESET_PIN as output pin
    GPIO_setAsOutputPin(LCD_RESET_PORT,LCD_RESET_PIN);
    // Reset is active low. Clear reset for 3usec to meet reset minimum
    // pulse width.
    GPIO_setOutputLowOnPin(LCD_RESET_PORT,LCD_RESET_PIN);
    __delay_cycles(SYSTEM_CLOCK_SPEED * .000003);
    GPIO_setOutputHighOnPin(LCD_RESET_PORT,LCD_RESET_PIN);
    
    // Configure LCD_CS_PIN as output. CS is active low
    GPIO_setOutputLowOnPin(LCD_CS_PORT,LCD_CS_PIN);
    GPIO_setAsOutputPin(LCD_CS_PORT,LCD_CS_PIN);

    // Configure LCD_CD_PIN as output and clear output to set it in command
    // mode
    GPIO_setAsOutputPin(LCD_CD_PORT, LCD_CD_PIN);
    GPIO_setOutputLowOnPin(LCD_CD_PORT, LCD_CD_PIN);

    // Configure LCD_MOSI_PIN
    GPIO_setAsPeripheralModuleFunctionOutputPin(LCD_MOSI_PORT,
        LCD_MOSI_PIN);
    
    // // Configure LCD_CLK_PIN  option select CLK
    GPIO_setAsPeripheralModuleFunctionOutputPin(LCD_CLK_PORT, LCD_CLK_PIN);

    // Initialize USCI_B1 for SPI Master operation
    USCI_B_SPI_masterInit(USCI_B1_BASE,
        USCI_B_SPI_CLOCKSOURCE_SMCLK,
#ifdef DRIVERLIB_LEGACY_MODE
	   UCS_getSMCLK(UCS_BASE),
#else
	   UCS_getSMCLK(),
#endif
        12500000,
        USCI_B_SPI_MSB_FIRST,
        USCI_B_SPI_PHASE_DATA_CAPTURED_ONFIRST_CHANGED_ON_NEXT,
        USCI_B_SPI_CLOCKPOLARITY_INACTIVITY_LOW
        );
   
    USCI_B_SPI_enable(USCI_B1_BASE);
    
    USCI_B_SPI_clearInterruptFlag(USCI_B1_BASE,
                           USCI_B_SPI_RECEIVE_INTERRUPT
                           );
}

//*****************************************************************************
//
//! Initializes the display driver.
//!
//! This function initializes the UC1701 display controller on the panel,
//! preparing it to display data.
//!
//! \return None.
//
//*****************************************************************************

void
Dogs102x64_UC1701Init(void)
{
    InitGPIOLCDInterface();     

    WriteCommand(SET_SCROLL_LINE + 0x00);
#if (defined LANDSCAPE)     // 6:00 viewing angle
    WriteCommand(SET_SEG_DIRECTION + SEG_MIRROR);
    WriteCommand(SET_COM_DIRECTION + COM_NORMAL);
#else                       // 12:00 viewing angle
    WriteCommand(SET_SEG_DIRECTION + SEG_NORMAL);
    WriteCommand(SET_COM_DIRECTION + COM_MIRROR);
#endif
    WriteCommand(SET_ALL_PIXEL_ON + DISABLE);
    WriteCommand(SET_INVERSE_DISPLAY + INVERSE);
    WriteCommand(SET_LCD_BIAS_RATIO + NINTH);
    WriteCommand(SET_POWER_CONTROL + BOOSTER + REGULATOR + FOLLOWER);
    WriteCommand(SET_VLCD_RESISTOR_RATIO + INTERNAL_RESISTOR_RATIO);
    WriteCommand(SET_ELECTRONIC_VOLUME_MSB);
    WriteCommand(SET_ELECTRONIC_VOLUME_LSB + ELECTRONIC_VOLUME_PM);
    WriteCommand(SET_ADV_PROGRAM_CONTROL0_MSB);
    WriteCommand(SET_ADV_PROGRAM_CONTROL0_LSB + TEMP_COMP_11);
    WriteCommand(SET_DISPLAY_ENABLE + ENABLE);

    
    Dogs102x64_backlightInit();
    Dogs102x64_setBacklight(ucBacklightLevel);
    Dogs102x64_setContrast(ucContrast);
    Dogs102x64_fillScreen(0x01);
}

//*****************************************************************************
//
//! Disables the display driver.
//!
//! This function disables the UC1701 display controller on the panel,
//! and also clears display data.
//!
//! \return None.
//
//*****************************************************************************
void Dogs102x64_disable(void)
{
    Dogs102x64_fillScreen(0x01);
    WriteCommand(SET_DISPLAY_ENABLE + ENABLE);
}

//*****************************************************************************
//
//! Inverts the display color.
//!
//! This function inverts the colors displayed without changing
//! any data in the buffer
//!
//! \return None.
//
//*****************************************************************************
void Dogs102x64_InverseDisplay(void)
{
    WriteCommand(SET_INVERSE_DISPLAY + REGULAR);
}

//*****************************************************************************
//
//! Sets display color back to the default.
//!
//! This function sets the display color back to normal
//!
//! \return None.
//
//*****************************************************************************
void Dogs102x64_ClearInverseDisplay(void)
{
    WriteCommand(SET_INVERSE_DISPLAY + INVERSE);
}

//*****************************************************************************
//
//! Initializes the display driver for playing LaunchpadDefender.
//!
//! This function initializes the UC1701 display controller on the panel,
//! preparing it to display data.
//!
//! \return None.
//
//*****************************************************************************
void Dogs102x64_DefenderInit(void)
{
    WriteCommand(SET_SCROLL_LINE + 0x00);
#if (defined LANDSCAPE)     // 6:00 viewing angle
    WriteCommand(SET_SEG_DIRECTION + SEG_MIRROR);
    WriteCommand(SET_COM_DIRECTION + COM_MIRROR);
#else                       // 12:00 viewing angle
    WriteCommand(SET_SEG_DIRECTION + SEG_NORMAL);
    WriteCommand(SET_COM_DIRECTION + COM_NORMAL);
#endif
    WriteCommand(SET_ALL_PIXEL_ON + DISABLE);
    WriteCommand(SET_INVERSE_DISPLAY + REGULAR);
    WriteCommand(SET_LCD_BIAS_RATIO + NINTH);
    WriteCommand(SET_POWER_CONTROL + BOOSTER + REGULATOR + FOLLOWER);
    WriteCommand(SET_VLCD_RESISTOR_RATIO + INTERNAL_RESISTOR_RATIO);
    WriteCommand(SET_ELECTRONIC_VOLUME_MSB);
    WriteCommand(SET_ELECTRONIC_VOLUME_LSB + ELECTRONIC_VOLUME_PM);
    WriteCommand(SET_ADV_PROGRAM_CONTROL0_MSB);
    WriteCommand(SET_ADV_PROGRAM_CONTROL0_LSB + TEMP_COMP_11);
    WriteCommand(SET_DISPLAY_ENABLE + ENABLE);
    
    Dogs102x64_backlightInit();
    Dogs102x64_setBacklight(ucBacklightLevel);
    Dogs102x64_setContrast(ucContrast);
    Dogs102x64_fillScreen(0x00);  
}
//*****************************************************************************
//
//! Fill screen with specified color
//!
//! This function fills the entire screen with white or black pixels
//! and changes the corresponding pixels in the buffer as well
//!
//! \return None.
//
//*****************************************************************************
void Dogs102x64_fillScreen(uint32_t ulValue)
{
  uint16_t i, j;
  
  // White Pixels
  if(ulValue)
  {
    //Page Address
    for(i = 0; i < 8; i++)
    {
      SetAddress(0,i);
      //Column Address
      for(j = 0; j < 102; j++)
      {
        DOGS102x64Memory[i * 102 + j] = 0xFF;
        WriteData(0xFF);
      }
    }
  }
  // Black Pixels
  else
  {
    //Page Address
    for(i = 0; i < 8; i++)
    {
      SetAddress(0,i);
      //Column Address
      for(j = 0; j < 102; j++)
      {
        DOGS102x64Memory[i * 102 + j] = 0x00;
        WriteData(0x00);
      }
    }
  }
}

//*****************************************************************************
//
//! Initialize the backlight.
//!
//! This function initializes the backlight of the display
//!
//! \return None.
//
//*****************************************************************************
void Dogs102x64_backlightInit(void)
{
    // Turn on Backlight
    // Uses PWM to control brightness
    GPIO_setAsPeripheralModuleFunctionOutputPin(LCD_BACKLT_PORT,
    		LCD_BACKLIGHT_PIN);

    // start at full brightness (8)
    TIMER_B_initCompare(TIMER_B0_BASE,
                      TIMER_B_CAPTURECOMPARE_REGISTER_4,
                      TIMER_B_CAPTURECOMPARE_INTERRUPT_DISABLE,
                      TIMER_B_OUTPUTMODE_RESET_SET,
                      (TIMER_B_getCaptureCompareCount(TIMER_B0_BASE,
                                                   TIMER_B_CAPTURECOMPARE_REGISTER_0
                                                   ) >> 1 )
                                                   
                      );

    TIMER_B_startUpMode(   TIMER_B0_BASE,
        TIMER_B_CLOCKSOURCE_ACLK,
        TIMER_B_CLOCKSOURCE_DIVIDER_1,
        50,
        TIMER_B_TBIE_INTERRUPT_DISABLE,
        TIMER_B_CAPTURECOMPARE_INTERRUPT_DISABLE,
        TIMER_B_SKIP_CLEAR
        );
    
}

//*****************************************************************************
//
//! Sets the backlight.
//!
//! This function sets the backlight level of the display,
//! from a level of 0-11 where 11 is the brightest setting.
//!
//! \return None.
//
//*****************************************************************************
void Dogs102x64_setBacklight(uint8_t brightness)
{
    uint16_t dutyCycle = 0, i, dummy;

    if (brightness > 0)
    {
    	GPIO_setAsPeripheralModuleFunctionOutputPin(LCD_BACKLT_PORT,
    			LCD_BACKLIGHT_PIN);

    	TIMER_B_initCompare(TIMER_B0_BASE,
    	    			TIMER_B_CAPTURECOMPARE_REGISTER_4,
    	    			TIMER_B_CAPTURECOMPARE_INTERRUPT_DISABLE,
    	    			TIMER_B_OUTPUTMODE_RESET_SET,
    	    			0);


    	dummy = TIMER_B_getCaptureCompareCount(TIMER_B0_BASE,
    	        		TIMER_B_CAPTURECOMPARE_REGISTER_0) >>4;

        dutyCycle = 12;

        for (i = 0; i < brightness; i++){
            dutyCycle += dummy;
        }

        TIMER_B_setCompareValue(TIMER_B0_BASE,
                		TIMER_B_CAPTURECOMPARE_REGISTER_4,
                		dutyCycle);

        //If the backlight was previously turned off, turn it on.
        if (!ucBacklightLevel){
        		TIMER_B_startCounter(TIMER_B0_BASE,
        	            			TIMER_B_UP_MODE);
        }
    }
    else
    {
    	TIMER_B_stop(TIMER_B0_BASE);
    	GPIO_setAsOutputPin(LCD_BACKLT_PORT,LCD_BACKLIGHT_PIN);
    	GPIO_setOutputLowOnPin(LCD_BACKLT_PORT,LCD_BACKLIGHT_PIN);
    }
    ucBacklightLevel = brightness;
}

//*****************************************************************************
//
//! Sets the contrast.
//!
//! This function sets the contrast of the UC1701 display controller,
//! from a level of 0-31 where 31 is the darkest setting.
//!
//! \return None.
//
//*****************************************************************************
void Dogs102x64_setContrast(uint8_t newContrast)
{
    //check if parameter is in range
    if (newContrast > 31)
    {
        newContrast = 31;
    }

    WriteCommand(SET_ELECTRONIC_VOLUME_MSB);
    WriteCommand(SET_ELECTRONIC_VOLUME_LSB + newContrast);
    ucContrast = newContrast;
}

//*****************************************************************************
//
//! Draws a non-standard grlib image.
//!
//! This function draws an image to  the UC1701 display with
//! optimum speed for playing the LaunchPad Defender game
//!
//! \return None.
//
//*****************************************************************************
void Dogs102x64_DefenderDraw(const uint8_t *pucData, int16_t lY, int16_t lX)
{
    uint8_t ucHeight, ucWidth;
    uint16_t ulBufferLocation, ulTempWidth;
    uint16_t i = 0;
  
    ulBufferLocation = lY * 102 + lX;
  
    ucWidth = *pucData++;
    ucHeight = *pucData++;
    
    ulTempWidth = ucWidth;
    while(ucHeight--)
    {
      // Set Cursor Address
      SetInverseAddress(lX, lY + i);
  
      while(ulTempWidth--)
      {
          DOGS102x64Memory[ulBufferLocation] = *pucData++;
          WriteData(DOGS102x64Memory[ulBufferLocation++]);
      }
      
      i++;
      ulTempWidth = ucWidth;
    }
}

//*****************************************************************************
//
//! Draws a pixel on the screen.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//! \param lX is the X coordinate of the pixel.
//! \param lY is the Y coordinate of the pixel.
//! \param ulValue is the color of the pixel.
//!
//! This function sets the given pixel to a particular color.  The coordinates
//! of the pixel are assumed to be within the extents of the display.
//!
//! \return None.
//
//*****************************************************************************
void
Dogs102x64_UC1701PixelDraw(void *pvDisplayData, int16_t lX, int16_t lY,
                                   uint16_t ulValue)
{  
  uint8_t ulPageAddress, ulPixelHeight;
  uint16_t ulBufferLocation;
  
  ulPageAddress = lY/8;
  ulPixelHeight = 0x01 << (lY & 0x07);
  ulBufferLocation = ulPageAddress * 102 + lX;
  
  // White Pixel
  if(ulValue)
  {
    DOGS102x64Memory[ulBufferLocation] |= ulPixelHeight;  
  }
  // Black Pixel
  else
  {
    DOGS102x64Memory[ulBufferLocation] &= ~ulPixelHeight;
  }
  
  SetAddress(lX, ulPageAddress);
  WriteData(DOGS102x64Memory[ulBufferLocation]);
}

//*****************************************************************************
//
//! Draws a horizontal sequence of pixels on the screen.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//! \param lX is the X coordinate of the first pixel.
//! \param lY is the Y coordinate of the first pixel.
//! \param lX0 is sub-pixel offset within the pixel data, which is valid for 1
//! or 4 bit per pixel formats.
//! \param lCount is the number of pixels to draw.
//! \param lBPP is the number of bits per pixel; must be 1, 4, or 8.
//! \param pucData is a pointer to the pixel data.  For 1 and 4 bit per pixel
//! formats, the most significant bit(s) represent the left-most pixel.
//! \param pucPalette is a pointer to the palette used to draw the pixels.
//!
//! This function draws a horizontal sequence of pixels on the screen, using
//! the supplied palette.  For 1 bit per pixel format, the palette contains
//! pre-translated colors; for 4 and 8 bit per pixel formats, the palette
//! contains 24-bit RGB values that must be translated before being written to
//! the display.
//!
//! \return None.
//
//*****************************************************************************
static void
Dogs102x64_UC1701PixelDrawMultiple(void *pvDisplayData, int16_t lX,
                                           int16_t lY, int16_t lX0, int16_t lCount,
                                           int16_t lBPP,
                                           const uint8_t *pucData,
                                           const uint16_t *pucPalette)
{   
  uint8_t ulPageAddress, ulPixelHeight;
  uint16_t ulBufferLocation;
  uint16_t Byte, ulValue; 
  
  ulPageAddress = lY/8;
  ulPixelHeight = 0x01 << (lY & 0x07);   
  ulBufferLocation = ulPageAddress * 102 + lX;
  
  //
  // Set the cursor increment to left to right, followed by top to bottom.
  //
  SetAddress(lX, ulPageAddress);
  
  //
  // Determine how to interpret the pixel data based on the number of bits
  // per pixel. Only 1BPP is supported on this display
  //
  if(lBPP == 1)
  {
      // The pixel data is in 1 bit per pixel format
    
      // Loop while there are more pixels to draw
      while(lCount > 0)
      {
        // Get the next byte of image data
        Byte = *pucData++;
          
        // Loop through the pixels in this byte of image data
        for(; (lX0 < 8) && lCount; lX0++, lCount--)
        {
          ulValue = ((uint16_t *)pucPalette)[(Byte >> (7 - lX0)) & 1];
          
          // White Pixel
          if(ulValue)
          {
            DOGS102x64Memory[ulBufferLocation] |= ulPixelHeight;  
          }
          // Black Pixel
          else
          {
            DOGS102x64Memory[ulBufferLocation] &= ~ulPixelHeight;
          }
          WriteData(DOGS102x64Memory[ulBufferLocation++]);
        }
        
        // Start at the beginning of the next byte of image data
        lX0 = 0;
      }
      // The image data has been drawn
  }
}

//*****************************************************************************
//
//! Draws a horizontal line.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//! \param lX1 is the X coordinate of the start of the line.
//! \param lX2 is the X coordinate of the end of the line.
//! \param lY is the Y coordinate of the line.
//! \param ulValue is the color of the line.
//!
//! This function draws a horizontal line on the display.  The coordinates of
//! the line are assumed to be within the extents of the display.
//!
//! \return None.
//
//*****************************************************************************
static void
Dogs102x64_UC1701LineDrawH(void *pvDisplayData, int16_t lX1, int16_t lX2,
                                   int16_t lY, uint16_t ulValue)
{
  
  uint8_t ulPageAddress, ulPixelHeight;
  uint16_t ulBufferLocation;
  
  ulPageAddress = lY/8;
  ulPixelHeight = 0x01 << (lY & 0x07);   
  ulBufferLocation = ulPageAddress * 102 + lX1;
  
  // Only need to set this address once,
  // the cursor is auto-incremented horizontally
  SetAddress(lX1, ulPageAddress);
  
  while(lX1++ <= lX2)
  {
    // White Pixels
    if(ulValue)
    {
      DOGS102x64Memory[ulBufferLocation] |= ulPixelHeight;  
    }
    // Black Pixels
    else
    {
      DOGS102x64Memory[ulBufferLocation] &= ~ulPixelHeight;
    }
    WriteData(DOGS102x64Memory[ulBufferLocation++]);
  }
}

//*****************************************************************************
//
//! Draws a vertical line.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//! \param lX is the X coordinate of the line.
//! \param lY1 is the Y coordinate of the start of the line.
//! \param lY2 is the Y coordinate of the end of the line.
//! \param ulValue is the color of the line.
//!
//! This function draws a vertical line on the display.  The coordinates of the
//! line are assumed to be within the extents of the display.
//!
//! \return None.
//
//*****************************************************************************
static void
Dogs102x64_UC1701LineDrawV(void *pvDisplayData, int16_t iX, int16_t iY1,
                                   int16_t iY2, uint16_t ulValue)
{
  uint8_t ulPageAddress1, ulPageAddress2, ulPixelHeight1, ulPixelHeight2;
  uint16_t ulBufferLocation;
  
  ulPageAddress1 = iY1/8;
  ulPageAddress2 = iY2/8;
  
  ulPixelHeight1 = 0xFF << (iY1 & 0x07);   
  ulPixelHeight2 = 0xFF >> (7 - (iY2 & 0x07));
  ulBufferLocation = ulPageAddress1 * 102 + iX; 
  
  //Vertical Line spans more than 1 page on the LCD
  if(ulPageAddress1 != ulPageAddress2)
  {    
    //Write First Page of vertical Line
    SetAddress(iX, ulPageAddress1);
    
    // White Pixels
    if(ulValue)
    {
      DOGS102x64Memory[ulBufferLocation] |= ulPixelHeight1;
    }
    // Black Pixels
    else
    {
      DOGS102x64Memory[ulBufferLocation] &= ~ulPixelHeight1;
    }
    WriteData(DOGS102x64Memory[ulBufferLocation]);
    
    // Skip to next page
    ulPageAddress1++;
    ulBufferLocation += 102;
    
    //Write Pages between First and Last Page
    while(ulPageAddress1 < ulPageAddress2)
    {
      SetAddress(iX, ulPageAddress1);
      
      // White Pixels
      if(ulValue)
      {
        DOGS102x64Memory[ulBufferLocation] |= 0xFF;
      }
      // Black Pixels
      else
      {
        DOGS102x64Memory[ulBufferLocation] &= ~0xFF;
      }
      WriteData(DOGS102x64Memory[ulBufferLocation]);
      
      // Skip to next page
      ulPageAddress1++;
      ulBufferLocation += 102;
    }
    
    //Write Last Page of vertical Line
    SetAddress(iX, ulPageAddress2);
    
    // White Pixels
    if(ulValue)
    {
      DOGS102x64Memory[ulBufferLocation] |= ulPixelHeight2;
    }
    // Black Pixels
    else
    {
      DOGS102x64Memory[ulBufferLocation] &= ~ulPixelHeight2;
    }
    WriteData(DOGS102x64Memory[ulBufferLocation]);
  }
  //Vertical line resides on one page on the LCD
  else
  {
    ulPixelHeight1 &= ulPixelHeight2;
    SetAddress(iX, ulPageAddress1);
    
    // White Pixels
    if(ulValue) 
    {
      DOGS102x64Memory[ulBufferLocation] |= ulPixelHeight1;
    }
    // Black Pixels
    else
    {
      DOGS102x64Memory[ulBufferLocation] &= ~ulPixelHeight1;
    }
    WriteData(DOGS102x64Memory[ulBufferLocation]);
  }
}

//*****************************************************************************
//
//! Fills a rectangle.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//! \param pRect is a pointer to the structure describing the rectangle.
//! \param ulValue is the color of the rectangle.
//!
//! This function fills a rectangle on the display.  The coordinates of the
//! rectangle are assumed to be within the extents of the display, and the
//! rectangle specification is fully inclusive (in other words, both sXMin and
//! sXMax are drawn, along with sYMin and sYMax).
//!
//! \return None.
//
//*****************************************************************************
static void
Dogs102x64_UC1701RectFill(void *pvDisplayData, const tRectangle *pRect,
                                  uint16_t ulValue)
{
  int16_t x0 = pRect->sXMin;
  int16_t x1 = pRect->sXMax;
  int16_t y0 = pRect->sYMin;
  int16_t y1 = pRect->sYMax;
  uint8_t ulPageAddress1, ulPageAddress2, ulPixelHeight1, ulPixelHeight2;
  uint16_t ulBufferLocation;
  
  ulPageAddress1 = y0/8;
  ulPageAddress2 = y1/8;
  
  ulPixelHeight1 = 0xFF << (y0 & 0x07);   
  ulPixelHeight2 = 0xFF >> (7 - (y1 & 0x07));
  ulBufferLocation = ulPageAddress1 * 102 + x0; 
  
  // Vertical Line spans more than 1 page on the LCD
  if(ulPageAddress1 != ulPageAddress2)
  {    
    // Write First Page of vertical Line
    SetAddress(x0, ulPageAddress1);
    
    // White Pixels
    if(ulValue)
    {
      // Draw across page
      for(; x0 <= x1; x0++)
      {
        DOGS102x64Memory[ulBufferLocation] |= ulPixelHeight1;
        WriteData(DOGS102x64Memory[ulBufferLocation++]);
      }
    }
    // Black Pixels
    else
    {
      for(; x0 <= x1; x0++)
      {
        // Draw across page
        DOGS102x64Memory[ulBufferLocation] &= ~ulPixelHeight1;
        WriteData(DOGS102x64Memory[ulBufferLocation++]);
      }
    }
    
    // Skip to next page and reset variables
    ulPageAddress1++;
    x0 = pRect->sXMin;
    ulBufferLocation = ulPageAddress1 * 102 + x0; 
    
    // Write Pages between First and Last Page
    while(ulPageAddress1 < ulPageAddress2)
    {
      SetAddress(x0, ulPageAddress1);
      
      // White Pixels
      if(ulValue)
      {
        // Draw across page
        for(; x0 <= x1; x0++)
        {
          DOGS102x64Memory[ulBufferLocation] = 0xFF;
          WriteData(DOGS102x64Memory[ulBufferLocation++]);
        }
      }
      // Black Pixels
      else
      {
        for(; x0 <= x1; x0++)
        {
          // Draw across page
          DOGS102x64Memory[ulBufferLocation] = 0x00;
          WriteData(DOGS102x64Memory[ulBufferLocation++]);
        }
      }
      
      // Skip to next page and reset variables
      ulPageAddress1++;
      x0 = pRect->sXMin;
      ulBufferLocation = ulPageAddress1 * 102 + x0; 
    }
    
    //Write Last Page of vertical Line
    SetAddress(x0, ulPageAddress2);
    
    // White Pixels
    if(ulValue)
    {
      // Draw across page
      for(; x0 <= x1; x0++)
      {
        DOGS102x64Memory[ulBufferLocation] |= ulPixelHeight2;
        WriteData(DOGS102x64Memory[ulBufferLocation++]);
      }
    }
    // Black Pixels
    else
    {
      for(; x0 <= x1; x0++)
      {
        // Draw across page
        DOGS102x64Memory[ulBufferLocation] &= ~ulPixelHeight2;
        WriteData(DOGS102x64Memory[ulBufferLocation++]);
      }
    }
  }
  //Vertical line resides on one page on the LCD
  else
  {
    ulPixelHeight1 &= ulPixelHeight2;
    SetAddress(x0, ulPageAddress1);
    
    // White Pixels
    if(ulValue)
    {
      // Draw across page
      for(; x0 <= x1; x0++)
      {
        DOGS102x64Memory[ulBufferLocation] |= ulPixelHeight1;
        WriteData(DOGS102x64Memory[ulBufferLocation++]);
      }
    }
    // Black Pixels
    else
    {
      for(; x0 <= x1; x0++)
      {
        // Draw across page
        DOGS102x64Memory[ulBufferLocation] &= ~ulPixelHeight1;
        WriteData(DOGS102x64Memory[ulBufferLocation++]);
      }
    }
  }
}

//*****************************************************************************
//
//! Clear the screen
//!
//! This function "clears" by filling the entire screen with white pixels
//! and changes the corresponding pixels in the buffer as well
//!
//! \return None.
//
//*****************************************************************************
void Dogs102x64_UC1701ClearDisplay(void *pvDisplayData, uint16_t ulValue)
{
  int16_t i, j;
  
  //Page Address
  for(i = 0; i < 8; i++)
  {
    SetAddress(0,i);
    //Column Address
    for(j = 0; j < 102; j++)
    {
      DOGS102x64Memory[i * 102 + j] = 0xFF;
      WriteData(0xFF);
    }
  }
}
//*****************************************************************************
//
//! Translates a 24-bit RGB color to a display driver-specific color.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//! \param ulValue is the 24-bit RGB color.  The least-significant byte is the
//! blue channel, the next byte is the green channel, and the third byte is the
//! red channel.
//!
//! This function translates a 24-bit RGB color into a value that can be
//! written into the display's frame buffer in order to reproduce that color,
//! or the closest possible approximation of that color.
//!
//! \return Returns the display-driver specific color.
//
//*****************************************************************************
static uint16_t
Dogs102x64_UC1701ColorTranslate(void *pvDisplayData,
                                        uint32_t ulValue)
{
    //
    // Translate from a 24-bit RGB color to a Black or White 1BPP color.
    //
    return(DPYCOLORTRANSLATE(ulValue));
}

//*****************************************************************************
//
//! Flushes any cached drawing operations.
//!
//! \param pvDisplayData is a pointer to the driver-specific data for this
//! display driver.
//!
//! This functions flushes any cached drawing operations to the display.  This
//! is useful when a local frame buffer is used for drawing operations, and the
//! flush would copy the local frame buffer to the display. This function isn't
//! implemented because each driver operation updates the buffer and the display
//!
//! \return None.
//
//*****************************************************************************
static void
Dogs102x64_UC1701Flush(void *pvDisplayData)
{
  // No operation
}

//*****************************************************************************
//
//! The display structure that describes the driver for the DOGS
//! 102x64 LCD with an UC1701 controller.
//
//*****************************************************************************
const tDisplay g_sDogs102x64_UC1701 =
{
    sizeof(tDisplay),
    DOGS102x64Memory,
#if defined(PORTRAIT) || defined(PORTRAIT_FLIP)
    64,
    102,
#else
    102,
    64,
#endif
    Dogs102x64_UC1701PixelDraw,
    Dogs102x64_UC1701PixelDrawMultiple,
    Dogs102x64_UC1701LineDrawH,
    Dogs102x64_UC1701LineDrawV,
    Dogs102x64_UC1701RectFill,
    Dogs102x64_UC1701ColorTranslate,
    Dogs102x64_UC1701Flush,
    Dogs102x64_UC1701ClearDisplay
};

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************
