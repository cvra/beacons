//---------------------------------------------------------------------------
// Header files
#include "radio.h"
#include "Define.h"

#include <p30f6011A.h>               //Standard header file
#ifdef DEBUG_RADIO
#include "LCD_display.h"
#endif

//---------------------------------------------------------------------------
// Definitions

#define Fcy 7372800                 //7.37MHz oscillator with 4xPLL -> 7.37MIPs
#define Sending1 IEC0bits.T1IE
#define Timer1Enable T1CONbits.TON 
#define Timer2 IEC0bits.T2IE
#define Timer2Enable T2CONbits.TON 

#define RADIO_BAUDRATE 19200		// desired baud rate
#define FCY  7372800		// xtal = 7.3728Mhz; PLLx4
#define LF	0x0A
#define CR	0x0D
#define NULL 0x00
#define WRITE_CHAR	0xA8
#define HOME_CLEAR	0x82
#define CURSOR_ON	0x8C
#define OFFSET_CENTER_BIT (Fcy / RADIO_BAUDRATE)/5
#define PERIOD_19200 (Fcy / RADIO_BAUDRATE)	


//---------------------------------------------------------------------------
//Variables
	#ifdef EMISSION_RADIO
	unsigned int  Message[MAX_LGR_MESSAGE + 5]; //Message convertis en mode 12 bits (Pr�ambule + start + taille + message + CRC)
	unsigned char * BrutMessageSending[MAX_LGR_MESSAGE + 2]; //Message � envoyer (message + CRC)
	#endif

    unsigned int MessageReceive[MAX_LGR_MESSAGE + 2];
    unsigned char BrutMessageReceive[MAX_LGR_MESSAGE];
	void (*OnMessageReceive)(void) = NULL;


    unsigned char NumByte = 0; //Compteur du byte en traitement
    unsigned int NumBit = 0; //Numero du bit en cours	  

    unsigned char StartBit = 0;
    unsigned char NbStartBits = 0;
   
    unsigned char BitsReceiveCount = 0;
    signed char BytesReceiveCount = -1;

    unsigned char LastBit = 0;

	unsigned char LgrMessage = 0;
   
   //Tableau utilis� pour la conversion 12 bits
    unsigned char BTbl[16] =  {13,3,19,21,22,25,26,28,35,37,38,41,42,44,50,52};

#ifdef EMISSION_RADIO
void ConvertMessageTo12Bits()
{
	unsigned char i;
	Message[0] = 2730; //pr�ambule
    Message[1] = 510; //startbit

	//Message[2] = LgrMessage;

	char Nibble = ((unsigned char)LgrMessage >> 4);
	//Nibble = Nibble & 15;
	Message[2] = (BTbl[Nibble] << 6);
	Nibble = (unsigned char)LgrMessage & 0x0F; //r�cuperer seulement la 2eme partie du message
	Message[2] = (Message[2] & 0xFC0) | (BTbl[Nibble] & 0x3F);


    for (i=0; i < LgrMessage + 2; i++)
	{
		Nibble = ((unsigned char)BrutMessageSending[i] >> 4);
		//Nibble = Nibble & 15;
		Message[i+3] = (BTbl[Nibble] << 6);
		Nibble = (unsigned char)BrutMessageSending[i] & 0x0F; //r�cuperer seulement la 2eme partie du message
		Message[i+3] = (Message[i+3] & 0xFC0) | (BTbl[Nibble] & 0x3F);
	}
}
#endif

#ifdef RECEPTION_RADIO
unsigned char GetBTblIndex(unsigned char Nibble)
{
    unsigned char i;
    for (i = 0; i <= 15; i++)
    {
        if (Nibble == BTbl[i])
        {
            return i;
        }
    }
    return '?';              
}

unsigned char Convert12BitsToMessage(unsigned int Brut)
{    
    return (((GetBTblIndex((Brut & 0xFC0) >> 6) << 4) & 0xF0) | GetBTblIndex(Brut & 0x3F));             
}

void DecodeMessage()
{
	unsigned char i;
	for (i=1; i <= LgrMessage; i++)
	{	
		BrutMessageReceive[i - 1] = Convert12BitsToMessage(MessageReceive[i]);
	}
}

#ifdef DEBUG_RADIO
void ShowMessage()
{
	unsigned char i;
	LCD_Display_Byte(HOME_CLEAR);	// clear LCD and put cursor at home
	for (i=0; i < MAX_LGR_MESSAGE; i++)
	{
  		LCD_Display_Byte(WRITE_CHAR);	// display on LCD
		LCD_Display_Byte(BrutMessageReceive[i]);
    }
}
#endif
#endif

unsigned int ByteCRC16(unsigned char value, unsigned int crcin)
{
    unsigned int k = (((crcin >> 8) ^ value) & 255) << 8;
    unsigned int crc = 0;
    unsigned int bits = 8;
    do
    {
        if (( crc ^ k ) & 0x8000)
            crc = (crc << 1) ^ 0x1021;
        else
            crc <<= 1;
        k <<= 1;
    }
    while (--bits);
    return ((crcin << 8) ^ crc);
}

#ifdef EMISSION_RADIO
void CalculateCRC()
{
	unsigned char i;
	unsigned int CRC = 0;
	for (i=0; i<LgrMessage; i++)
	{
		CRC = ByteCRC16((unsigned char)BrutMessageSending[i],CRC);		
	}
	BrutMessageSending[LgrMessage] = (CRC >> 8);
	BrutMessageSending[LgrMessage + 1] = ((unsigned char)CRC);
}
#endif

#ifdef RECEPTION_RADIO
unsigned int CalculateReceiveCRC()
{
	unsigned char i;
	unsigned int CRC = 0;
	for (i=0; i < LgrMessage; i++)
	{
		CRC = ByteCRC16(BrutMessageReceive[i],CRC);
	}
	return CRC;
}
#endif


//---------------------------------------------------------------------------
// Main routine
//---------------------------------------------------------------------------
void InitializeRadio(void (*DoMessageReceive)(void))
{
//------------------------------------------------------------------------------
//Initialize Timer1
#ifdef EMISSION_RADIO
   T1CON = 0;                    //Turn off Timer1 by clearing control register
   TMR1 = 0;                     //Start Timer1 at zero
   PR1 = PERIOD_19200;            //Set Timer1 period register value for 1 second for debug
   T1CON = 0x0000;               //Configure Timer1 (timer off, continue in IDLE, not gated, 1:1 prescaler, internal clock)
   IPC0bits.T1IP = 7;
#endif

//------------------------------------------------------------------------------
//Initiliaze Timer 2
#ifdef RECEPTION_RADIO
   T2CON = 0;                    //Turn off Timer2 by clearing control register
   TMR2 = 0;                     //Start Timer2 at zero
   PR2 = (Fcy / 19200)/2;        //Set Timer2 2 fois plus lentement que le timer 1 
   T2CON = 0x0000;               //Configure Timer2 (timer off, continue in IDLE, not gated, 1:1 prescaler, internal clock)
   IPC1bits.T2IP = 7;
#endif

//------------------------------------------------------------------------------
//Set Timers interrupt priority and enable interrupt
   
   IFS0bits.T1IF = 0;            //Clear Timer1 interrupt flag
   IFS0bits.T2IF = 0;            //Clear Timer2 interrupt flag

   #ifdef RECEPTION_RADIO
   #ifdef DEBUG_RADIO
   LCD_Display_Setup();	// Init LCD display
   LCD_Display_Byte(HOME_CLEAR);	// clear LCD and put cursor at home
   LCD_Display_Byte(CURSOR_ON); // turn on cursor
   #endif
   #endif
   
   NbStartBits = 0;
   StartBit = 0;
   Sending1 = 1; //Activer l'interrupt sur le timer1
   Timer2 = 1;
   	
   #ifdef RECEPTION_RADIO
	//Regler flanc montant!!!
	//INTCON2bits.INT1EP = 0;
	//IEC1bits.INT1IE = 0;
	
	CNEN1bits.CN1IE = 1;
	IEC0bits.CNIE = 0;
	

	Timer2Enable = 1;
   #endif
	OnMessageReceive = DoMessageReceive;	


	//Gestion des bits de controles
	#ifdef EMISSION_RADIO
		LATCbits.LATC1 = 0;
		LATCbits.LATC2 = 1;
	#endif 

	#ifdef RECEPTION_RADIO
		LATCbits.LATC1 = 1;
		LATCbits.LATC2 = 1;
	#endif 

}

//---------------------------------------------------------------------------
// Timer 1 Interrupt Service Routine
// A chaque Timer1 envoyer le bit suivant
//---------------------------------------------------------------------------
#ifdef EMISSION_RADIO
void _ISR _T1Interrupt(void)
{
   
   if (LastBit)
   {
       LATCbits.LATC14 = 1;
	   Timer1Enable = 0;
       LastBit = 0;
   }
   else
   {
       unsigned int SendBit = 0;
 
       SendBit = (Message[NumByte] & NumBit);
	
       LATCbits.LATC14 = SendBit ? 0 : 1;
   
       if (NumBit == 1)
       {
          NumBit = 2048;     
          NumByte++;
          if (NumByte > (LgrMessage + 4))
          {
             LastBit = 1;
          }
       }
       else
       {
         NumBit = NumBit >> 1;
       }
   }
 
    IFS0bits.T1IF = 0;            //Clear Timer1 interrupt flag
}
#endif

#ifdef EMISSION_RADIO
void SendMessage()
{
   while(Timer1Enable == 1);
   CalculateCRC();
   ConvertMessageTo12Bits();
   NumBit = 2048;
   NumByte = 0;
   TMR1 = 0;
   Timer1Enable = 1; //D�marre l'envoi du message
}
#endif

#ifdef RECEPTION_RADIO
void _ISR _T2Interrupt(void)
{
	//DEBUT DE L'INTERRUPT
	LATGbits.LATG1 = 1;
	if (StartBit == 0)
	{
        if (PORTCbits.RC13 == 0)
		{
			NbStartBits++;
			if (NbStartBits == 12)	
			{
                //DEBUG: STARTBIT DETECTE
				LATGbits.LATG8 = 1;
				StartBit = 1;                  //qui se d�clenchera sur le flanc descendant
                Timer2Enable = 0;
				IFS0bits.CNIF = 0;		   //Effacer le flag poru l'interrupt INT1 
				IEC0bits.CNIE = 1;    	   	    //enable INT1 interrupt
                BitsReceiveCount = 0;
				BytesReceiveCount = -1;
            }    
		}
		else
		{
             NbStartBits = 0;
		}
	}
	else
	{
	  if ((BitsReceiveCount == 0) && (BytesReceiveCount == -1))
	  {
		 Timer2Enable = 0;
		 PR2 = PERIOD_19200;
		 Timer2Enable = 1;
      }
		
	  if (BitsReceiveCount % 12 == 0)
	  {
	     BytesReceiveCount++;
      }

	  BitsReceiveCount++;
	  
	  unsigned char NewBit = 0;	  

	  //DEBUG LECTURE DU BIT
	  LATGbits.LATG7 = 1;
	  if (PORTCbits.RC13 == 1)
   	  {
		  NewBit = 0; 
		  LATGbits.LATG9 = 0;
	  }
      else
	  {
         NewBit = 1;
		 LATGbits.LATG9 = 1;	
	  }
	  LATGbits.LATG7 = 0;

	  MessageReceive[BytesReceiveCount] = (((MessageReceive[BytesReceiveCount] << 1) & 4094) | NewBit);	 

	  if (BitsReceiveCount == 12)
	  {
	  	  LgrMessage = Convert12BitsToMessage(MessageReceive[0]);
		  if (LgrMessage > MAX_LGR_MESSAGE)
		  {
			 LgrMessage = MAX_LGR_MESSAGE;
		  }		  
	  }	  

	  if (BitsReceiveCount == ((LgrMessage + 3) *12))
	  {
	      NbStartBits = 0;
          StartBit = 0;
		  DecodeMessage();
		  
		  #ifdef DEBUG_RADIO
			  ShowMessage();
		  #endif

		  unsigned int ReceiveCRC;
		  #ifndef BALISE_ESPION
		  ReceiveCRC = CalculateReceiveCRC();
		  #endif

		  #ifdef BALISE_ESPION		
		  if (1 == 1)
		  #else
		  if (((ReceiveCRC >> 8) == Convert12BitsToMessage(MessageReceive[LgrMessage + 1])) && (((unsigned char)ReceiveCRC) == Convert12BitsToMessage(MessageReceive[LgrMessage + 2])))		 
		  #endif
		  {
			 OnMessageReceive();	
		  }
		  else
		  {
			 	if (LATGbits.LATG15 == 0)
				{
					LATGbits.LATG15 = 1;
				}
				else
				{
					LATGbits.LATG15 = 0;
				}	//Allum� LED 3	
		  }
			 /*//OK
			 LCD_Display_Byte(WRITE_CHAR);	// display on LCD
			 LCD_Display_Byte('O');
			 LCD_Display_Byte(WRITE_CHAR);	// display on LCD
			 LCD_Display_Byte('K');
		
		  }	
		  else
		  {
			 //KO
			 LCD_Display_Byte(WRITE_CHAR);	// display on LCD
			 LCD_Display_Byte('K');
			 LCD_Display_Byte(WRITE_CHAR);	// display on LCD
			 LCD_Display_Byte('O');
		  }*/
		  PR2 = (Fcy / 19200)/2;
       }    
	}
	//DEBUG FIN DE L'INTERRUPT
	LATGbits.LATG1 = 0;
    IFS0bits.T2IF = 0;            //Clear Timer2 interrupt flag
}
#endif

#ifdef RECEPTION_RADIO
void _ISR _CNInterrupt(void) //Declare INT1 interrupt function
{
   PR2 = PERIOD_19200 + OFFSET_CENTER_BIT;
   TMR2 = 0;                 //On resette le compteur
   Timer2Enable = 1;
   IFS0bits.CNIF = 0; 	 //Clear INT1 interrupt flag
   IEC0bits.CNIE = 0;
}
#endif
