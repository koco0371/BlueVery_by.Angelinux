#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>

#include "usbdrv/usbdrv.h"

#include <util/delay.h>
#include <string.h>

//USB제어 파이프를 통해 입력받은 request 메세지의 종류 
#define ERROR_CODE 0	
#define BOOT_INIT 1

static uint8_t RxBuffer[20]; //키보드로부터 전달받은 값을 저장하는 버퍼
static uint8_t replybuf[5]; //USB제어파이프를 통해 전송하는 버퍼
static uint8_t intbuf[8]; //USB인터럽트 파이프를 통해 전송하는 버퍼
volatile unsigned char Rxflag = 0; //키보드로부터 값을 충분히 수신받았음을 알리는 flag
volatile unsigned int Rxcur = 0; //RxBuffer에 정보를 저장하는 커서
static uint8_t LedState; //capslock의 LEDstate정보를 나타내는 변수

void BT_Init(unsigned long xtal, unsigned long bps); //블루투스와의 전송관련 설정을 초기화하는 함수
void BT_PutChar(char byData); //블루투스 모듈로 (키보드)로 데이터를 전송하는 함수

// this gets called when custom control message is received
USB_PUBLIC uchar usbFunctionSetup(uchar data[8]) { 
	usbRequest_t *rq = (void *) data; //받은 데이터를 rq에 대입

	replybuf[0] = rq->bRequest; //reply 버퍼의 첫 인덱스를 전송받은 request값으로 채움
	if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_VENDOR) { //만약 request의 타입이 일치할 경우
		if(rq->bRequest == BOOT_INIT) //request가 BOOT_INIT일 경우 
			replybuf[1] = BOOT_INIT; //reply버퍼의 두번째 인덱스를 BOOT_INIT으로 채움
		else //그 외는 에러
			replybuf[1] = ERROR_CODE; //reply버퍼의 두번째 인덱스를 EEROR_CODE로 채움
		usbMsgPtr = (unsigned char *) replybuf; //repl

  	return 2; //replybuffer에 들어간 데이터의 크기반환
	}
	//일치하지 않을 경우 -> CAPSLOCK 제어 
	return USB_NO_MSG; //usbFunctionWrite함수 호출
}
usbMsgLen_t usbFunctionWrite(uint8_t * data, uchar len) {
	if (data[0] == LedState) //data[0]의 값이 기존 LedState값과 일치할 경우 함수 종료
  	return 1;
  else //다를 경우 LedState 값 갱신
  	LedState = data[0];
 
	BT_PutChar(LedState); //갱신된 LedState값 전송
	BT_PutChar(0x0D);
  
	return 1; // Data read, not expecting more
}

ISR(USART_RXC_vect) { //블루토스모듈(키보드)를 통해 데이터를 전송받을때 인터럽트 발생
	RxBuffer[Rxcur] = UDR; //전송받은 데이터를 RxBuffer에 저장
	switch(RxBuffer[Rxcur]) { //전송받은 데이터의 종류를 파악
		case 0x0D: //블루투스모듈 자체에서 보댄 데이터의 끝을 알림
			if(Rxcur < 8) { //RxBuffer의 길이가 8보다 짧을 경우
				if(strncmp("+READY",(char *)RxBuffer,6) == 0) //RxBuffer의 내용이 READY일 경우 쓰레기값 Rxflag해제
					Rxflag = 0;
				else  //그 외의 내용은 인터럽트파이프로 PC에 전송해야함 (ERROR)
					intbuf[1] = 0x04;
			}
			else //RxBuffer의 길이가 8이상일 경우
			{
				if(strncmp("+CONNECTED", (char *)RxBuffer,10) == 0) //RxBuffer의 내용이 CONNECTED일 경우 
				{	
					intbuf[1] = 0x02; //인터럽트파이프로 PC에 해당내용을 전송해야함
					LedState = 0x0F; //LedState에 쓰레기값 대입
				}
				else if(strncmp("+DISCONNECTED", (char *)RxBuffer, 13) == 0) //RxBuffer의 내용이 DISCONNECTED일 경우 
					intbuf[1] = 0x03; //인터럽트파이프로 PC에 해당내용을 전송해야함
				else if(strncmp("+ADVERTISING", (char *)RxBuffer,12) == 0) //RxBuffer의 내용이 ADVERTISING일 경우 
					Rxflag = 0; //쓰레기값이므로 Rxflag해제
				else //그 외의 내용은 인터럽트파이프로 PC에 전송해야함 (ERROR)
					intbuf[1] = 0x04;
			}	
			Rxflag = 1; //Rxflag 1로 설정
			Rxcur = 0; //RxBuffer의 커서 0으로 초기화
			break;

		case 0xEF: //키보드에서 보낸 데이터의 끝을 알림
			intbuf[0] = RxBuffer[0]; //특수키들의 입력값을 0번 인덱스에 대입
			intbuf[1] = 0x01; //PC에 키보드에서 전달된 값임을 알리기 위해
			for(int i = 2; i <= Rxcur; i++) //키보드에 눌려진 키들의 스캔코드를 대입
				intbuf[i] = RxBuffer[i-1]; 

			Rxcur = 0; 
			Rxflag = 1; //Rxflag 1로 설정 //RxBuffer의 커서 0으로 초기화
			break;

		case 0xFE: //키보드에서 LedState값을 요청함
			BT_PutChar(LedState); //LedState값 전송
			BT_PutChar(0x0D);
			Rxcur = 0; //RxBuffer의 커서 0으로 초기화
			break;

		default: //아직 전송 미완료
			Rxcur++; //RxBuffer의 커서 옮김
			break;
	}
}

int main() {
		uint16_t i;
    wdt_enable(WDTO_1S); // enable 1s watchdog timer

    usbInit();

    usbDeviceDisconnect(); // enforce re-enumeration
    for(i = 0; i<250; i++) { // wait 500 ms
        wdt_reset(); // keep the watchdog happy
        _delay_ms(2);
    }
		BT_Init(F_CPU,38400);
    usbDeviceConnect();
  	
	  sei(); // Enable interrupts after re-enumeration
	
    while(1) {
        wdt_reset(); // keep the watchdog happy
        usbPoll();
				
				if(Rxflag == 1) { //Rxflag가 1로 설정될 경우
					if(usbInterruptIsReady()) { //인터럽트파이프가 준비될 경우 intbuf에 쌓은 데이터 전송
						usbSetInterrupt(intbuf,8);
					}
					Rxflag = 0; //Rxflag 해제
				}
    }
	
    return 0;
}

void BT_Init(unsigned long xtal, unsigned long bps) {
  unsigned long temp;

  UCSRB = (1 << TXEN) | (1 << RXEN);
  UCSRB |= (1 << RXCIE);

  temp = xtal/(bps * 16UL) - 1;

	UBRRL = temp & 0xFF;
  UBRRH = (temp >> 8) & 0xFF;
}
void BT_PutChar(char byData) {
  while(!(UCSRA & (1 << UDRE)));
  UDR = byData;
}
