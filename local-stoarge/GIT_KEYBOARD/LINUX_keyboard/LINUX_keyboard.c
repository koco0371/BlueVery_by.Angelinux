#include<avr/io.h>
#include<avr/interrupt.h>
#include<util/delay.h>
#include<string.h>

volatile unsigned char key_x = 8; //현재 입력값을 얻어낼 키보드의 x좌표값
volatile unsigned char Rxflag = 0; //블루투스로부터 입력이 완료됨을 알리는 flag
volatile unsigned int Rxcur = 0; //RxBuffer에 값을 저장하는 인덱스값
volatile unsigned char RxBuffer[20]; //RxBuffer의
volatile unsigned int Txflag = 0;
volatile unsigned int Txcur = 0;
volatile unsigned char TxBuffer[70];
volatile unsigned char real_key_x;

void Serial_Init(unsigned long xtal, unsigned long bps);
void Serial_PutChar(char byData);

/*
포트사용 
PORTB
0 CAPSLOCK LED 출력 
1 RIGHTSHIFT 입력
2 LEFTSHIFT 입력
3
4
5
PORTC
0 키패드 입력 1
1 키패드 입력 2
2 키패드 입력 3
3 키패드 입력 4
4 키패드 입력 5
5 CTRL 입력
6 
PORTD
0 RX
1 TX
2 ALT 입력
3 키패드 출력 1
4 키패드 출력 2
5 키패드 출력 3
6 키패드 출력 E1
7 키패드 출력 E2
*/

ISR(USART_RXC_vect)
{
	RxBuffer[Rxcur] = UDR;
	if(RxBuffer[Rxcur] == 0x0D)
	{
		if(strncmp("+CONNECTED", (char *)RxBuffer,10) == 0)
		{	
			_delay_ms(100);
			Serial_PutChar(0xFE);
			Serial_PutChar(0x0D);
		}
		Rxcur = 0;
	}
	else if(RxBuffer[Rxcur] == 0x00 || RxBuffer[Rxcur] == 0x01)
	{
		PORTB = RxBuffer[Rxcur];
		Rxcur = 0;
	}
	else if(Rxcur >= 19)
	{
		Rxcur = 0;
	}
	else
		Rxcur++;
}
int main()
{
	DDRB = 0x01;
	DDRC = 0x00;
	DDRD = 0xF8;
	PORTB = 0x00;

	Serial_Init(F_CPU, 38400);
  sei();
	while(1) {
		Txcur = 1;
		for(key_x = 8; key_x <=21; key_x++) {
			PORTD = (key_x << 3);
			_delay_us(100);

			real_key_x = ((key_x - 8) << 4);
			if(!(PINC&0x01))
				TxBuffer[Txcur++] = real_key_x + 0x04;
			if(!(PINC&0x02))
				TxBuffer[Txcur++] = real_key_x + 0x03;
			if(!(PINC&0x04))
				TxBuffer[Txcur++] = real_key_x + 0x02;
			if(!(PINC&0x08))
				TxBuffer[Txcur++] = real_key_x + 0x01;
			if(!(PINC&0x10))
				TxBuffer[Txcur++] = real_key_x + 0x00;
		}
		if(Txcur > 4)
		{
			Txcur = 1;
			Txflag = 1;
		}
		if((Txflag >= 1 && Txflag <= 10) || Txcur > 1) {
			TxBuffer[0] = ((!(PIND&0x04))<< 2) + ((!(PINC&0x20))<< 1) +  ((!(PINB&0x02))<< 3) +  (!(PINB&0x04));
			for(int i = 0; i < Txcur; i++)
				Serial_PutChar(TxBuffer[i]);
			for(int i = Txcur; i < 3; i++)
				Serial_PutChar(0xEE);
			Serial_PutChar(0xEF);
			Serial_PutChar(0x0D);
			if(Txcur == 1)
			{
				if(Txflag < 10)
					Txflag++;
				else
					Txflag = 0;
			}
			else
				Txflag = 1;
		}
		_delay_ms(10);
	}
	
	return 0;
}
void Serial_Init(unsigned long xtal, unsigned long bps)
{
  unsigned long temp;

  UCSRB = (1 << TXEN) | (1 << RXEN);
  UCSRB |= (1 << RXCIE);

  temp = xtal/(bps * 16UL) - 1;

  UBRRH = (temp >> 8) & 0xFF;
	UBRRL = temp & 0xFF;;
}
void Serial_PutChar(char byData)
{
  while(!(UCSRA & (1 << UDRE)));
  UDR = byData;
}
