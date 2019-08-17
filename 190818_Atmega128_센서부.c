#include <stdlib.h>
#include <delay.h>
#include <mega128.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

void StartOut(void);
void DHT_Read(void);
void getRawData();
void getAcclDegree(void);
void getGyroDegree(void);
void compFilter(void);
unsigned char MPU6050_read(unsigned char addr);
void MPU6050_write(unsigned char addr, char data);

char bit_data[41]; //dht11 에서 수신되는 데이터가 총 start bit(1)+ data bit(40) 41byte
volatile int DHT_cnt=0;
int StDone=0;       //시작을 알리는 신호의 끝을 알리는 플래그
int toggle=0;
int time=0;
int humi=0;
int temp=0;
int Humidflag0=0, Humidflag1=0;   // APP에서 "0" 을 수신 받았다는 것을 확인 하기 위한 변수

volatile int gx = 0, gy = 0, gz = 0, ax = 0, ay = 0, az = 0;                 //자이로 센서에서 사용하는 변수
volatile long x_aTmp1, y_aTmp1, z_aTmp1;
volatile float x_aTmp2, y_aTmp2, z_aTmp2, x_aResult, y_aResult, z_aResult;
volatile float x_gTmp1, y_gTmp1, x_gResult, y_gResult, z_gTmp1, z_gResult;
float kp = 12.0f, ki = 1.0f;
volatile float xTmp1, yTmp1, xTmp2, yTmp2, xIntTmp1, yIntTmp1    ;
volatile float xFilterAngle = 0.0f, yFilterAngle = 0.0f;
volatile int pitch = 0, roll = 0, yaw=0;
bit gyr0_flag=0;    // APP에서 "0" 을 수신 받았다는 것을 확인 하기 위한 변수
bit gyr1_flag=0;    // APP에서 "1" 을 수신 받았다는 것을 확인 하기 위한 변수
int adc1,avr1,dif;   // 자이로 센서 값을 평균 내기 위한 변수
int Wakecnt=0,Sleepcnt=0;           // 자이로 센서 값을 평균 내기 위해 일정 시간 센서 값을 받아들일 시간을 만드는 변수
long sum1=0;        // 자이로 센서 값을 평균 내기 위한 변수

int count=0;
int i=0;
     

unsigned char buffer[12];

void Init_Timer1(void)
{
    TCCR1A = 0x00;
    TCCR1B = 0x01;
}

void Init_Timer3()
{
    TCCR3B = 0b00000100; //분주비를 clk / 256으로 설정하였습니다. (시간을 구하는 방법 또한 아래서 자세히!!)
 
    TIMSK = (1<<TOIE3); //타이머/카운터1 의 오버플로우 개별 인터럽트를 허용합니다.
 
    TCNT3H = 0xFB; //1초의 오버플로우를 만들기 위한 타이머/카운터1의 초기값을 정해줍니다.
    TCNT3L = 0x1D;
}

interrupt [EXT_INT4] void external_int4(void)   //Atmega128보드에서 Start신호를 보낸 뒤 데이터를 구분해주는 인터럽
{
    if(toggle==0)\
    {
        TIMSK = 0x04;            // TOIE0 = '1';    
        TCNT1 = 0;               // 타이머/카운터0 레지스터 초기값
        EICRB=0b00000010;    // ISC41=1, ISC40=0 ☞ 하강엣지시 인터럽트 작동(INT4)
        toggle = 1;
    } 
    else if(toggle==1)
    {
        TIMSK = 0x04;            // TOIE0 = '1';    
        EICRB=0b00000011;    // ISC41=1, ISC40=1 ☞ 상승엣지시 인터럽트 작동(INT4)
        time=TCNT1;
        if(time>=100*8) bit_data[DHT_cnt++]='1';
        else if(time<100*8) bit_data[DHT_cnt++]='0';     
        toggle = 0;
    }
}

interrupt [TIM1_OVF] void timer_int1(void)
{
   TIMSK = 0x04;            // TOIE0 = '1';
    EIMSK=0b00000000;    // 외부 인터럽트4 인에이블 
    StDone=1;  
}

interrupt [TIM3_OVF] void timer0_ovf_isr (void) //핵심 인터럽트 걸릴 시 이곳으로 점프!
{
    count++;
 
    TCNT3H = 0xFB; //1초의 오버플로우를 만들기 위한 타이머/카운터1의 초기값을 정해줍니다.
    TCNT3L = 0x1D;
}

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
    while ( !( UCSR1A & 0x20 ) )   // (1<<UDRE) 
    ;
    // Put data into buffer, sends the data
    UDR1 = data;
}

interrupt [USART1_RXC] void usart1_rx_isr(void)
{
    unsigned char ch;
    ch = UDR1;
    USART0_Transmit(ch);              
} 

interrupt [USART0_RXC] void usart0_rx_isr(void)
{
    unsigned char mp;
    mp = UDR0;
    USART1_Transmit(mp);
}

void main(void)
{
    TWSR = 0x00;
    TWBR = 12;   //SCK:400000Hz
    USART_Init(); // 9600/8/n/1/n
    Init_Timer1();
    Init_Timer3();  
    EICRB = 0x03;
    SREG |= 0x80;
    
    MPU6050_write(0x6B,0x00);
    MPU6050_write(0x6C,0x00);
   
    getRawData(); 
    getAcclDegree();
    getGyroDegree();
    compFilter();
    for(i=0;i<10;i++)  // i가 10이 될 때 까지 roll 값을 sum1 에 저장
    { 
    delay_ms(1); 
    sum1+=roll; 
    }            
    avr1=sum1/10; 

    while(1){ 
            DHT_Read();
            getRawData();              
            getAcclDegree();
            getGyroDegree();            
            compFilter(); 
            adc1=roll;
            sum1-=avr1;  
            sum1+=adc1;           // 새로운 값으로 바꿔서 더한 후
            avr1=sum1/10;         // 평균을 다시 계산
            dif=adc1-avr1;          // 평균 값과 제일 최근에 들어온 값의 차이를 추출    
           
             if(count%300==0)
            { 
            if((dif<-3)||(dif>3))  // 평균과의 차이가 -5 이하 & 5 이상 이 아니면 15번동안 count 하면서 값을 추출
            {
               Sleepcnt =0;
            if(gyr1_flag==0)
            {      
            USART1_Transmit( 'B' );
            gyr0_flag=0;
            gyr1_flag=1;                         
            } 
            } 
            else
            { 
            if(Sleepcnt<15)     // 평균과의 차이가 -5 이하 & 5 이상 이 아니면 15번동안 count 하면서 값을 추출
            {
            Sleepcnt++;   
            }
            else if(gyr0_flag==0)    // 15번동안 값을 비교했는데 움직임이 감지가 안되면 수면으로 간주
            {                  
            USART1_Transmit( 'A' );
            gyr0_flag=1;
            gyr1_flag=0;  
            }          
            }
            }
            
            if(Humidflag1==0)
            {
            if(humi>90){
            USART1_Transmit('H' );
            Humidflag0=0;
            Humidflag1=1;
            }
            }
        
            else if(Humidflag0==0)
            {
            if(humi<=90)
            {
            USART1_Transmit('V' );
            Humidflag0=1;
            Humidflag1=0;
            }
            }
    }
  }
  
void StartOut(void) //Atmega보드에서 dht11에 시작을 알리는 신호
{ 
    DDRE.4 = 0xFF;
    PORTE.4 = 0xFF;
    delay_ms (100); // 100ms High 유지
    PORTE.4 = 0x00;
    delay_ms (18); // 18ms Low 유지
    PORTE.4 = 0xFF;
    delay_us (30); //약 30us 동안 '1'유지
    PORTE.4 = 0x00;
    DDRE.4 = 0x00; // 응답신호와 데이터 신호를 받기 위해 입력으로 설정
     
    EICRB=0b00000011;    // ISC41=1, ISC40=1 ☞ 상승엣지시 인터럽트 작동(INT4)
    EIMSK=0b00010000;    // 외부 인터럽트4 인에이블 
    DHT_cnt=0;
}

void DHT_Read(void) //DHT11 data 40byte를 read
{
    
    delay_ms(100);
    StartOut();
    while(StDone==0);
    StDone = 0;
        
    humi = 0;
    temp = 0;
        
    for(i=0;i<8;i++)
    {
        if(bit_data[i+1]=='1')
            humi+=1<<(7-i);
    }
        
    for(i=0;i<8;i++)
    {
        if(bit_data[i+17]=='1')
            temp+=1<<(7-i);
    }
}  
  
  unsigned char MPU6050_read(unsigned char addr)
{
    unsigned char dat;
    TWCR = 0xA4;// ((1<<TWINT)|(1<<TWSTA)|(1<<TWEN)) -> TWEN으로 TWI 허가 / TWSTA는 마스터로서의 동작, 버스사용이 가능하면 START 조건 출력 / TWINT TW인터럽트 
    while(((TWCR & 0x80) == 0x00 || ((TWSR & 0xF8) != 0x08)));// TWCR레지스터의 TWINT가 0 이거나 TWSR의 TWS3비트가 1이 아니라면 계속 반복
    TWDR = 0xD0;
    TWCR = 0x84;// ((1<<TWINT)|(1<<TWEN)) -> TWEN으로 TWI 허가 / TWINT TW인터럽트
    while(((TWCR & 0x80) == 0x00 || ((TWSR & 0xF8) != 0x18)));// TWCR레지스터의 TWINT가 0 이거나
    TWDR = addr;
    TWCR = 0x84;// ((1<<TWINT)|(1<<TWEN)) -> TWEN으로 TWI 허가 / TWINT TW인터럽트
    while(((TWCR & 0x80) == 0x00 || ((TWSR & 0xF8) != 0x28)));// TWCR레지스터의 TWINT가 0 이거나
    TWCR = 0xA4;// ((1<<TWINT)|(1<<TWSTA)|(1<<TWEN)) -> TWEN으로 TWI 허가 / TWSTA는 마스터로서의 동작, 버스사용이 가능하면 START 조건 출력 / TWINT TW인터럽트    
    //-------------------------------------------------------------
    while(((TWCR & 0x80) == 0x00 || ((TWSR & 0xF8) != 0x10)));// TWCR레지스터의 TWINT가 0 이거나
    TWDR = 0xD1;
    TWCR = 0x84;// ((1<<TWINT)|(1<<TWEN)) -> TWEN으로 TWI 허가 / TWINT TW인터럽트
    while(((TWCR & 0x80) == 0x00 || ((TWSR & 0xF8) != 0x40)));// TWCR레지스터의 TWINT가 0 이거나 
    TWCR = 0x84;// ((1<<TWINT)|(1<<TWEN)) -> TWEN으로 TWI 허가 / TWINT TW인터럽트 
    while(((TWCR & 0x80) == 0x00 || ((TWSR & 0xF8) != 0x58)));// TWCR레지스터의 TWINT가 0 이거나
    dat = TWDR;
    TWCR = 0x94;// ((1<<TWINT)|(1<<TWSTA)|(1<<TWEN)) -> TWEN으로 TWI 허가 / TWSTO는 마스터에선 TWI STOP, 슬레이브에선 SCL과 SDA 신호선을 High-Z상태로 하여 에러상태 해제 / TWINT TW인터럽트  
    return dat;
}
// 자이로 센서
void MPU6050_write(unsigned char addr, char data)
{
    TWCR = 0xA4;// ((1<<TWINT)|(1<<TWSTA)|(1<<TWEN)) -> TWEN으로 TWI 허가 / TWSTA는 마스터로서의 동작, 버스사용이 가능하면 START 조건 출력 / TWINT TW인터럽트 
    while(((TWCR & 0x80) == 0x00 || ((TWSR & 0xF8) != 0x08)));// TWCR레지스터의 TWINT가 0 이거나
    TWDR = 0xD0;
    TWCR = 0x84;// ((1<<TWINT)|(1<<TWEN)) -> TWEN으로 TWI 허가 / TWINT TW인터럽트 
    while(((TWCR & 0x80) == 0x00 || ((TWSR & 0xF8) != 0x18)));// TWCR레지스터의 TWINT가 0 이거나
    TWDR = addr; // addr = 0x43
    TWCR = 0x84;// ((1<<TWINT)|(1<<TWEN)) -> TWEN으로 TWI 허가 / TWINT TW인터럽트
    while(((TWCR & 0x80) == 0x00 || ((TWSR & 0xF8) != 0x28)));// TWCR레지스터의 TWINT가 0 이거나    
    //-------------------------------------------------------------  
    TWDR = data;
    TWCR = 0x84;// ((1<<TWINT)|(1<<TWEN)) -> TWEN으로 TWI 허가 / TWINT TW인터럽트
    while(((TWCR & 0x80) == 0x00 || ((TWSR & 0xF8) != 0x28)));// TWCR레지스터의 TWINT가 0 이거나
    TWCR = 0x94;// ((1<<TWINT)|(1<<TWSTA)|(1<<TWEN)) -> TWEN으로 TWI 허가 / TWSTO는 마스터에선 TWI STOP, 슬레이브에선 SCL과 SDA 신호선을 High-Z상태로 하여 에러상태 해제 / TWINT TW인터럽트
    delay_us(50);
}
// 자이로 센서
void getRawData()
{
    buffer[0] = MPU6050_read(0x3B);// ax-H
    buffer[1] = MPU6050_read(0x3C);// ax-L
    buffer[2] = MPU6050_read(0x3D);// ay-H
    buffer[3] = MPU6050_read(0x3E);// ay-L
    buffer[4] = MPU6050_read(0x3F);// az-H
    buffer[5] = MPU6050_read(0x40);// ax-L 
    
    buffer[6] = MPU6050_read(0x43);// gx-H
    buffer[7] = MPU6050_read(0x44);// gx-L
    buffer[8] = MPU6050_read(0x45);// gy-H
    buffer[9] = MPU6050_read(0x46);// gy-L
    buffer[10] = MPU6050_read(0x47);//gz-H
    buffer[11] = MPU6050_read(0x48);//gz-L  
    
    ax = (int)buffer[0] << 8 | (int)buffer[1];
    ay = (int)buffer[2] << 8 | (int)buffer[3];
    az = (int)buffer[4] << 8 | (int)buffer[5];
    gx = (int)buffer[6] << 8 | (int)buffer[7];
    gy = (int)buffer[8] << 8 | (int)buffer[9];
    gz = (int)buffer[10] << 8 | (int)buffer[11];
}
// 자이로 센서
void getAcclDegree(void)// 가속도 측정값들을 라디안 단위로
{
    x_aTmp1=((long)ay*(long)ay)+((long)az*(long)az);
    y_aTmp1=((long)ax*(long)ax)+((long)az*(long)az);
    z_aTmp1=((long)ay*(long)ay)+((long)az*(long)az);
    
    x_aTmp2=sqrt((float)x_aTmp1);
    y_aTmp2=sqrt((float)y_aTmp1);
    z_aTmp2=sqrt((float)z_aTmp1);
    
    x_aResult=atan((float)ax/x_aTmp2);
    y_aResult=atan((float)ay/y_aTmp2);
    z_aResult=atan(z_aTmp2/(float)az);//가속도 결과값 
}
// 자이로 센서 
void getGyroDegree(void)// 자이로 측정값들을 라디안 단위로
{
    x_gTmp1=(float)gx/65536;
    y_gTmp1=(float)gy/65536;
    z_gTmp1=(float)gz/65536;
    
    x_gTmp1=x_gTmp1*1.8;
    y_gTmp1=y_gTmp1*1.8;
    z_gTmp1=z_gTmp1*1.8;
    
    x_gResult=x_gTmp1;   //각속도 결과값
    y_gResult=y_gTmp1;
    z_gResult=z_gTmp1;
}
// 자이로 센서
void compFilter(void) // 불안정한 센서 값을 필터링 해주는 상보 필터
{
    xTmp1=(-y_aResult)+(float)xFilterAngle; // 현재 각도와, 최정 얻은 값의 차이를 얻음 (ERROR 값)
    xIntTmp1=(float)xIntTmp1+(xTmp1*0.01);  // ERROR 값을 적분함
    xTmp2=(-kp*xTmp1)+(-ki*(float)xIntTmp1)+x_gResult; 
    xFilterAngle=xFilterAngle+(xTmp2*0.01);  // 각속도 값의 적분
    pitch=(int)(xFilterAngle*180/PI);        // 적분된 값이 라디안 이므로 각도로 바꾸어 줌
    
    yTmp1=(-x_aResult)+(float)yFilterAngle;
    yIntTmp1=(float)yIntTmp1+(yTmp1*0.01);
    yTmp2=(-kp*yTmp1)+(-ki*(float)yIntTmp1)+y_gResult;
    yFilterAngle+=yTmp2*0.01;
    roll=(int)(yFilterAngle*180/PI);
  
    yaw=(int)(0.976*(yaw+((gz/131.)*0.001)))+(0.024*(atan2(ax, ay)*180/PI));
}
      

    
  


