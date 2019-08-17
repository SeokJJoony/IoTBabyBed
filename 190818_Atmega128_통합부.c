#define F_CPU 8000000UL
#include <mega128.h>
#include <delay.h>

typedef unsigned char INT8;
typedef unsigned int INT16;

#define BAUD 9600
#define U2X_S 2     // Set of U2X --> 1 or 2
#define MYUBRR ((F_CPU*U2X_S)/(16L*BAUD)-1)
#define sbi(reg,bit)    reg |= (1<<(bit))      // Set "bit"th bit of Register "reg"
#define cbi(reg,bit)    reg &= ~(1<<(bit))
#define MAXLEN 16
//
#define MP3_NEXT                    0x01
#define MP3_PREVIOUS                0x02
#define MP3_TRAKING_NUM                0x03 // 0..2999
#define MP3_INC_VOLUME                0x04
#define MP3_DEC_VOLUME                0x05
#define MP3_VOLUME                    0x06 // 0..30
#define MP3_EQ                        0x07 // 0-Normal / 1-Pop / 2-Rock / 3-Jazz / 4-Classic / 5-Base
#define MP3_PLAYBACK_MODE            0x08 // 0-Repeat / 1-folder repeat / 2-single repeat / 3-random
#define MP3_PLAYBACK_SOURCE            0x09 // 0-U / 1-TF / 2-AUX / 3-SLEEP / 4-FLASH
#define MP3_STANDBY                    0x0A
#define MP3_NORMAL_WORK                0x0B
#define MP3_RESET                    0x0C
#define MP3_PLAYBACK                0x0D
#define MP3_PAUSE                    0x0E
#define MP3_PLAY_FOLDER_FILE        0x0F // 0..10
#define MP3_VOLUME_ADJUST            0x10
#define MP3_REPEAT                    0x11 // 0-stop play / 1-start repeat play
// Query the System Parameters

////////////////////////////////////////////////////////////////////////////////
//Commands parameters
////////////////////////////////////////////////////////////////////////////////
#define MP3_EQ_Normal                    0
#define MP3_EQ_Pop                        1
#define MP3_EQ_Rock                        2
#define MP3_EQ_Jazz                        3
#define MP3_EQ_Classic                    4
#define MP3_EQ_Base                        5
#define MP3_PLAYBACK_MODE_Repeat        0
#define MP3_PLAYBACK_MODE_folder_repeat    1
#define MP3_PLAYBACK_MODE_single_repeat    2
#define MP3_PLAYBACK_MODE_random        3
#define MP3_PLAYBACK_SOURCE_U            0
#define MP3_PLAYBACK_SOURCE_TF            1
#define MP3_PLAYBACK_SOURCE_AUX            2
#define MP3_PLAYBACK_SOURCE_SLEEP        3
#define MP3_PLAYBACK_SOURCE_FLASH        4
 
INT8 default_buffer[10] = {0x7E, 0xFF, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xEF}; // Default Buffer
volatile INT8 mp3_cmd_buf[10] = {0x7E, 0xFF, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xEF};
volatile INT8 recv_buf[20] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
int buf_seg[4] = {0,0,0,0};     // 편의를 위한 전역 변수(버퍼) 생성 
unsigned int Port_char[] ={0xc0,0xf9,0xa4,0xb0,0x99,0x92,0x82,0xf8,0x80,0x90, 0x88, 0x83, 0xA7, 0xA1, 0x86, 0x8E, 0xFF};  // 0~F 의 문자표
unsigned int Port_fnd[] ={0x1f,0x2f,0x4f,0x8f,0x0f};    // FND0 ON, FND1 ON, FND2 ON, FND3 ON, ALL FND OFF
int Ten=0,One=0;
//unsigned char str[MAXLEN];
//unsigned char *ptr;
// Fnd 문자표 및 fnd포트 핀 설정

unsigned char ch;
int i=0,j=0,a = 0,delay=0;
int pos=0;
int posMax=0;
int z=0;

void SwingStop();
void SwingFirst();
void SwingSecond();
void SwingThird();    
void USART_Init( )
{
  UCSR1B = 0x98; // Receive interrupt
  UCSR0B = 0x98;    //Receive(RX) 및 Transmit(TX) Enable
  UCSR0C = UCSR1C = 0x06;    // UARTMode, 8 Bit Data, No Parity, 1 Stop Bit
  UBRR0H = UBRR1H = 0;       //16Mhz, baud rate = 115200 (if 9600, 0)
  UBRR0L = UBRR1L = 103;       //16Mhz, baud rate = 115200 (if 9600, 103)
}

void USART0_Transmit( char data )
{
    // Wait for empty transmit buffer
    while ( !( UCSR0A & 0x20 ) )   // (1<<UDRE) 
    ;
    // Put data into buffer, sends the data
    UDR0 = data;
}

void USART1_Transmit( char data )
{
    // Wait for empty transmit buffer
    while ( !( UCSR1A & 0x20 ) );   // (1<<UDRE) 
    
    // Put data into buffer, sends the data
    UDR1 = data;
} 

INT16 MP3_checksum (void)
{
    INT16 sum = 0;
    INT8 i;
    for (i=1; i<7; i++) {
        sum += mp3_cmd_buf[i];
    }
    return -sum;
}

void MP3_send_cmd (INT8 cmd, INT16 high_arg, INT16 low_arg)
{
    INT8 i;
    INT16 checksum;
    mp3_cmd_buf[3] = cmd;
    mp3_cmd_buf[5] = high_arg;
    mp3_cmd_buf[6] = low_arg;
    checksum = MP3_checksum();
    mp3_cmd_buf[7] = (INT8) ((checksum >> 8) & 0x00FF);
    mp3_cmd_buf[8] = (INT16) (checksum & 0x00FF);
    for( i=0; i< 10; i++){
        USART0_Transmit(mp3_cmd_buf[i]);
        //putchar(mp3_cmd_buf[i]);
        mp3_cmd_buf[i] = default_buffer[i];
    }
}

void dfplayer_init(void)
{
    MP3_send_cmd(MP3_PLAYBACK_SOURCE,0,MP3_PLAYBACK_SOURCE_TF);
    delay_ms(10);
    MP3_send_cmd(MP3_VOLUME, 0,10); 
    delay_ms(10);
}

void Init_Timer3A(void)
{
    TCCR3A = 0xAA;
    TCCR3B = 0x1A;
    
    OCR3AH = 3000>>8;
    OCR3AL = 3000&0xff;

    ICR3H = 39999>>8;
    ICR3L = 39999>>0xff;
}

void Servo_motor(int angle)
{
    OCR3AH = (angle*14+3640)>>8;
    OCR3AL = (angle*14+3640)&0xFF;
}

void Print_Segment(int buf_seg[])
{ 
    
    PORTE = Port_fnd[3]; 
    PORTB = Port_char[buf_seg[3]];  
   
 
               
}
   
void SwingStop(void)      
{
    if(buf_seg[3]>0)
    {
       while(1)
       {
       switch(posMax)
       {
       case 0:
          buf_seg[3]=0;
          break;
       case 8:
          buf_seg[3]=1;
          break;
       case 16:
          buf_seg[3]=2;
          break;
       case 24:
          buf_seg[3]=3;
          break; 
       case 32:
          buf_seg[3]=4;
          break;
       case 40:
          buf_seg[3]=5;
          break;
       case 48:
          buf_seg[3]=6;
          break;
       default:
          break;  
       } 
         
       Print_Segment(buf_seg);
       posMax-=4;
         
       for(pos=-posMax;pos<posMax;pos++)
        { 
        Servo_motor(pos);
        delay_ms(10);
        }
       
         for(pos=posMax;pos>-posMax;pos--)
        { 
        Servo_motor(pos);
        delay_ms(10);
        } 
        
       if(buf_seg[3]==0)
       {
       if(posMax<=0)
       break;
       }
       }
    }
    
}
    
void SwingFirst(void)
{
    if(buf_seg[3]>3)
    { 
        while(1)
       {  
        posMax-=4;
        
       switch(posMax)
       {
       case 0:
          buf_seg[3]=0;
          break;
       case 8:
          buf_seg[3]=1;
          break;
       case 16:
          buf_seg[3]=2;
          break;
       case 24:
          buf_seg[3]=3;
          break; 
       case 32:
          buf_seg[3]=4;
          break;
       case 40:
          buf_seg[3]=5;
          break;
       case 48:
          buf_seg[3]=6;
          break;
           case 56:
          buf_seg[3]=7;
          break;
       case 64:
          buf_seg[3]=8;
          break;
       case 72:
          buf_seg[3]=9;
          break;
       default:
          break;  
       }
          
       Print_Segment(buf_seg);
        
       for(pos=-posMax;pos<posMax;pos++)
        { 
        Servo_motor(pos);
        delay_ms(10);
        }

         for(pos=posMax;pos>-posMax;pos--)
        { 
        Servo_motor(pos);
        delay_ms(10);
        }
         
       if(buf_seg[3]==3)
       {
       if(posMax==24)
       break;
       } 

       }
    }


    if(buf_seg[3]==0)
    {
    while(1)
    {
    posMax+=4;

    switch(posMax)
    {
    case 8:
          buf_seg[3]=1;
          break;
    case 16:
          buf_seg[3]=2;
          break;
    case 24:
          buf_seg[3]=3;
          break;
    default:
     break;
    }         
      
    Print_Segment(buf_seg); 
         
    for(pos=-posMax;pos<posMax;pos++)
    { 
    Servo_motor(pos);
    delay_ms(10);
    }
    
    for(pos=posMax;pos>-posMax;pos--)
    { 
    Servo_motor(pos);
    delay_ms(10);
    }
    
    if(buf_seg[3]==3)
    {
    if(posMax==24)
    {
    break;
    }
    }
    }
} 

    
    if(buf_seg[3]==3)
    {
    if(posMax==24)
    {
            while(1)
            {   
                posMax=24;
                
                Print_Segment(buf_seg); 

                for(pos=-posMax;pos<posMax;pos++)
                { 
                Servo_motor(pos);
                delay_ms(10);
                }

                for(pos=posMax;pos>-posMax;pos--)
                { 
                Servo_motor(pos);
                delay_ms(10);
                }  
               if(z==0)
                {    
                SwingStop();
                break;
                }
                if(z==2)
                {
                 SwingSecond();
                 break;
                }
               if(z==3)
                {
                 SwingThird();
                 break;
                }
            }
    }
    }
}

void SwingSecond(void)
{
     if(buf_seg[3]>6)
    { 
        while(1)
       {  
        posMax-=4;
        
       switch(posMax)
       {
       case 0:
          buf_seg[3]=0;
          break;
       case 8:
          buf_seg[3]=1;
          break;
       case 16:
          buf_seg[3]=2;
          break;
       case 24:
          buf_seg[3]=3;
          break; 
       case 32:
          buf_seg[3]=4;
          break;
       case 40:
          buf_seg[3]=5;
          break;
       case 48:
          buf_seg[3]=6;
          break;
           case 56:
          buf_seg[3]=7;
          break;
       case 64:
          buf_seg[3]=8;
          break;
       case 72:
          buf_seg[3]=9;
          break;
       default:
          break;  
       }
          
       Print_Segment(buf_seg);       
         
       for(pos=-posMax;pos<posMax;pos++)
        { 
        Servo_motor(pos);
        delay_ms(10);
        }
       
         for(pos=posMax;pos>-posMax;pos--)
        { 
        Servo_motor(pos);
        delay_ms(10);
        } 
       if(buf_seg[3]==6)
       {
       if(posMax==48)
       break;
       }
       }
    }
    
     if(buf_seg[3]<6)
    {
    while(1)
    {
     posMax+=4;
    switch(posMax)
    {
    case  8:
          buf_seg[3]=1;
          break;
    case 16:
          buf_seg[3]=2;
          break;
    case 24:
          buf_seg[3]=3;
          break; 
    case 32:
          buf_seg[3]=4;
          break;
    case 40:
          buf_seg[3]=5;
          break;
    case 48:
          buf_seg[3]=6;
          break;
    default:
          break;
    }        
           
    Print_Segment(buf_seg); 
         
     for(pos=-posMax;pos<posMax;pos++)
    { 
    Servo_motor(pos);
    delay_ms(10);
    }
    
     for(pos=posMax;pos>-posMax;pos--)
    { 
    Servo_motor(pos);
    delay_ms(10);
    }
    
    if(buf_seg[3]==6)
    {
    if(posMax==48)
    {
    break;
    }
    }
    } 
    }
    
    if(buf_seg[3]==6)
    {
    if(posMax==48)
    {
            while(1)
            {   
                posMax=48;
                
            Print_Segment(buf_seg); 
          
            
                for(pos=-posMax;pos<posMax;pos++)
                { 
                Servo_motor(pos);
                delay_ms(10);
                }
           
                for(pos=posMax;pos>-posMax;pos--)
                { 
                Servo_motor(pos);
                delay_ms(10);
                }  
               if(z==0)
                {    
                SwingStop();
                break;
                }
                if(z==1)
                {
                 SwingFirst();
                 break;
                }
              if(z==3)
                {
                 SwingThird();
                 break;
                }
            }
    }
    }
}

void SwingThird(void)
{
   if(buf_seg[3]<9)
    {
    while(1)
    {
       posMax+=4;
    switch(posMax)
       {
       case 0:
          buf_seg[3]=0;
          break;
       case 8:
          buf_seg[3]=1;
          break;
       case 16:
          buf_seg[3]=2;
          break;
       case 24:
          buf_seg[3]=3;
          break; 
       case 32:
          buf_seg[3]=4;
          break;
       case 40:
          buf_seg[3]=5;
          break;
       case 48:
          buf_seg[3]=6;
          break;
           case 56:
          buf_seg[3]=7;
          break;
       case 64:
          buf_seg[3]=8;
          break;
       case 72:
          buf_seg[3]=9;
          break;
       default:
          break;  
       }   
          
            Print_Segment(buf_seg); 
         
    
     for(pos=-posMax;pos<posMax;pos++)
    { 
    Servo_motor(pos);
    delay_ms(10);
    }
    
     for(pos=posMax;pos>-posMax;pos--)
    { 
    Servo_motor(pos);
    delay_ms(10);
    }
    
    if(buf_seg[3]==9)
    {
    if(posMax==72)
    {
    break;
    }
    }
    } 
    }
    
    if(buf_seg[3]==9)
    {
    if(posMax==72)
    {
            while(1)
            {   
                posMax=72;
                
            Print_Segment(buf_seg); 
          
            
                for(pos=-posMax;pos<posMax;pos++)
                { 
                Servo_motor(pos);
                delay_ms(10);
                }
           
                for(pos=posMax;pos>-posMax;pos--)
                { 
                Servo_motor(pos);
                delay_ms(10);
                }  
                if(z==0)
                {    
                SwingStop();
                break;
                }
                if(z==1)
                {
                 SwingFirst();
                 break;
                }
                if(z==2)
                {
                 SwingSecond();
                 break;
                }
            }
    }
    }
}


void PORT_Init(void)
{
    DDRE.3 = 1;    
        //PORTE.4~PORTE.7 FND 출력  (PE.4 : FND0, PE.5, FND1, PE.6 : FND2, PE.7 : FND3)
    
    DDRE.7 = 1;
    DDRB = 0xff;    //세그먼트의 문자포트 출력(PB.0:a, PB.1:b, PB.2:c, PB.3:d, PB.4:e, PB.5:f, PB.6:g, PB.7:dot)    
    PORTE = 0x88;       
}

interrupt [USART1_RXC] void usart1_rx_isr(void)
{
              ch = UDR1;              
              switch(ch){
                 case 'N' :            
                    MP3_send_cmd(0x01,0,0);   //다음 
                    break;
                 case 'P' :
                    if(a==0)
                    {            
                      MP3_send_cmd(0x0E,0,0);    
                      a = 1;
                    }
                    else
                    {  
                      MP3_send_cmd(0x0D,0,0);    
                      a = 0;
                    }
                    break;
                 case 'U' :            
                    MP3_send_cmd(0x04,0,0);
                    break;
                 case 'L' :               
                    MP3_send_cmd(0x05,0,0);
                    break;
                 case 'D' :            
                    z = 0;   
                    break;
                 case 'E' :
                    z = 1;
                    break;
                 case 'F' :            
                    z = 2;
                    break;
                 case 'G' :               
                    z = 3;
                    break;
                 case '0' :
                 case '1' :
                 case '2' :
                 case '3' :
                 case '4' :
                 case '5' :
                 case '6' :
                 case '7' :
                 case '8' :
                 case '9' :
                     MP3_send_cmd(0x12,0,ch - 0x2F);
                 default  :
                    break;
              }
     
} 

void Check(INT8 end)
{
              int x; 
              USART1_Transmit( end + 0x60 );
              if(end == 10)
              { 
              MP3_send_cmd(0x12,0,1);
              }
              else
              {
                MP3_send_cmd(0x12,0,end + 0x01);
              }  
             
              for( x=0; x< 20; x++){
                     recv_buf[x] = 0x00; 
                  }
                  
}
interrupt [USART0_RXC] void usart0_rx_isr(void)
{          
              recv_buf[i] = UDR0;
              i++;     
              
              if(i == 20)
              {   
                i = 0;
                Check(recv_buf[6]);                
              }                       
}

void main(void)
{
    
    USART_Init(); // 9600/8/n/1/n
    dfplayer_init();
    Init_Timer3A();   
    PORT_Init(); 
    MP3_send_cmd(0x12,0,1);
    posMax=0;
    SREG = 0x80; 
     
    while(1)
    {
    
        for(delay=0;delay<=40;delay++)
        {
        Print_Segment(buf_seg); 
        }
         
        if(z==0)
        {
        SwingStop();
        }
        
        if(z==1)
        {       
        SwingFirst();      
        }
                  
        if(z==2)
        {        
        SwingSecond();
        }
        
         if(z==3)
        {        
        SwingThird();
        }
     }
}
