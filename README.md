# Bluevery

## Angelinux

> - [최인혁](https://github.com/Daniel9710)
> - [박성우](https://github.com/koco0371)
> - [우인혜](https://github.com/raspberry6523)
> - [이승진](https://github.com/lsjboy93)
***
### Proejct Summary

> Linux 기반의 디바이스 드라이버로, AVR 환경을 기반으로 만들어진 키보드를 활용하여 만들었습니다.
> ATmega-8A를 활용해 키보드를 제작했으며, 블루투스 통신은 USART 통신을 활용하여 제작했습니다.
> 개발 기간은 2019/08/13~2019/09/08입니다.
> 고유한 키로 (cd ..), (ls -l), (ps), (exit) 커맨드 키가 존재합니다.
> http://223.194.70.113:11322/ Gitea 서버에서 개발을 완료했습니다.

***
### Environment

<img src="https://miro.medium.com/max/4000/1*b_al7C5p26tbZG4sy-CWqw.png" width=300>
<img src="https://upload.wikimedia.org/wikipedia/commons/thumb/9/96/Avr_logo.svg/1200px-Avr_logo.svg.png" width=300>
<img src="https://www.skillagit.com/data/product/1572694066.jpg" width=300>
<img src="https://t1.daumcdn.net/cfile/tistory/9923B0495D66434618" width=300>
<img src="https://i2.wp.com/wp.laravel-news.com/wp-content/uploads/2016/12/laravel-valet-ubuntu.png?resize=2200%2C1125" width=300>


### Installation

1. Clone the repo

```sh
git clone https://github.com/koco0371/BlueVery_by.Angelinux.git
```

2. Insert module

```sh
$ make
$ insmod bt_usb-3.o
```

3. Run

```
블루투스 키보드를 usb에 삽입해 사용합니다
```
***
### Directory

```
.
└── local-storage
    ├── bt_usb-3.c
    └── GIT_KEYBOARD
        ├── LINUX_keyboard
        │   ├── LINUX_keyboard.c
        └── LINUX_USBreceiver
            └── main.c
    
```
***
### Source description

### Linux driver

> 리눅스에 insert되는 모듈

**1. bt_usb-3.c**

```
- bt_keyboard_probe
 드라이버를 위해 필요한 공간을 할당하는 함수
 input 장치, interrupt, control urb, DMA 버퍼, usb control pipe 등 
 드라이버에서 사용되는 장치 구조체인 bt_keyboard를 작성
 usb_device에 해당 장치를 등록
 control pipe를 통해 INIT 메시지를 송신하고 결과를 수신
 urb를 위한 정보 작성(콜백 함수, dma 버퍼, 컨트롤 파이프)

- bt_keyboard_alloc_mem
 urb, dma 버퍼, control request를 위한 메모리를 할당하는 함수

- bt_keyboard_free_mem
 할당받은 메모리를 해제하는 함수(urb, dma 버퍼, control request)

- bt_kbd_open
 interrupt urb에 usb_device를 등록
 interrupt urb를 사용가능한 상태로 제출

- bt_kbd_close
 interrupt urb를 사용불가능한 상태로 만듦.

- bt_keyboard_disconnect
 usb가 제거되었을 때 불리는 함수
 interrupt urb를 사용 불가능하게 만들고,
 input device 등록을 해제하고
 할당했던 메모리를 해제한다.
 등록된 usb device 갯수를 줄인다.

- bt_keyboard_event
 led input event가 발생하면 제일 먼저 호출되는 함수
 capslock에 대한 점등상태를 등록
 urb를 재설정하고 led urb를 사용 가능한 상태로 만든다.

- bt_keyboard_led
 bt_keyboard_event 실행 이후 불려지는 callback 함수
 제대로 led 상태가 등록이 되었는지를 체크하는 함수

- bt_keyboard_irq
 키보드 입력 이벤트(interrupt)가 발생하면 실행되는 함수
 dma 버퍼의 1번 원소를 확인하고, 1이 입력되었을 경우 bt_kbd_input_key 함수를 실행한다.
 2가 입력되었을 경우 bluetooth 연결시 capslock 상태를 전달하는 것이다.
 3이 입력되면 bluetooth 연결이 해제된 것이다.
 4가 입력되면 other command를 의미한다.

- bt_kbd_input_key
 dma 버퍼의 0번째 원소에서 shift, ctrl이 눌렸는지 여부를 판단한다.
 나머지 원소를 돌면서 지난 주기에 눌렸던 키가 이번 주기에 눌렸을/안 눌렸을 경우에 대한 처리를 진행한다.
 input_sync 함수를 통해 input_report_key로 쌓아놓은 input event를 한 번에 처리한다.
```
***
### AVR

> 하드웨어(키보드, usb 동글)를 위한 AVR 코드

**1. main.c**
> usb 동글 코드

```
- usbFunctionSetup
 custom된 control message가 수신되었을 때 실행되는 함수
 reply 버퍼의 첫 인덱스를 전송받은 request 값으로 채운다.
 request의 타입을 확인해서 USBRQ_TYPE_VENDOR와 일치할 경우 관련 처리를 진행한다.
 일치하지 않을 경우 capslock 제어이므로 usbFunctionWrite 함수를 호출한다.

- usbFunctionWrite
 전송된 led 상태가 기존의 led 상태와 일치하면 함수를 종료한다.
 아닐 경우 갱신된 led 상태를 키보드에 BT_PutChar 함수를 통해 전송한다.

- ISR (interrupt service routine)
 블루투스 모듈을 통해 키보드에서 데이터를 전송받으면 인터럽트 발생
 UDR을 통해 전송받은 데이터를 RxBuffer에 저장한다.
 전송받은 데이터가 0x0D일 경우, 블루투스 모듈 자체에서 데이터의 끝을 알리는 것이다.
 이 경우 버퍼의 길이가 8보다 짧을 경우와 이상일 경우로 나뉜다.
 8보다 짧을 경우, 버퍼의 내용이 READY일 경우 쓰레기 값을 의미하는 Rxflag를 설정한다
 READY가 아니면 에러이므로, interrupt 버퍼에 내용을 담아 PC에 해당 내용을 전송한다.
 8 이상일 경우, CONNECTED, DISCONNECTED, ADVERTISING, erro의 경우로 나뉘고, 해당 상황에 따른 처리를 진행한다.
 전송받은 데이터가 0xEF일 경우 키보드에서 보낸 데이터의 끝을 알리는 것이다.
 특수키(ctrl, shift)들의 입력값을 0번 인덱스에 대입하고, 1번에는 키보드에서 전달된 값임을 알리는 0x01을 삽입한다.
 나머지 원소에는 키보드에 눌려진 키들의 키코드를 대입한다.
 전송받은 데이터가 0xFE일 경우 키보드에서 Led State 값을 요청한 것이다.
 BT_PutChar를 통해 led state를 전송한다.

- BT_Init
 UCSRB를 통해 interrupt 수신을 가능하게 하고, 수신과 송신을 가능하게 조정한다.
 clock을 입력받은 값으로 설정한다.

- BT_PutChar
 UCSRA(상태 레지스터 A)에서 송신 버퍼가 비었는지 여부를 판단한다.
 송신버퍼가 비어 송신이 가능하게되면 UDR(송신 버퍼)를 통해 해당 정보를 송신한다.
```
---
**2. LINUX_keyboard.c**

```
- ISR(Interrupt Service routine)
 블루투스 모듈에서 컨트롤 메시지가 송신되었을 때 interrupt가 발생하여 호출되는 함수
 RxBuffer에 UDR을 통해 들어온 데이터를 삽입한다.
 RxBuffer에 들어있는 메시지가 0x0D인 경우 CONNECTED 메시지인지 확인한 후
 Serial_PutChar를 통해 0xFE, 0x0D를 돌려보낸다.(일종의 수신 확인)
 메시지가 0x00 혹은 0x01인 경우 led 상태를 변경하는 것이다.
 capslock의 led를 출력하는 PORTB에 해당 상태를 입력한다.

- main
 PIN들을 돌면서 해당 키가 눌렸는지를 체크하여 TxBuffer에 담는다.
 TxBuffer 0번에는 shift와 ctrl이 입력되었는지 여부를 비트 단위로 입력한다.
 Serial_PutChar를 통해 버퍼에 있는 내용을 송신하고 키의 송신이 완료되었다는 의미인 0xEF와 통신이 완료되었다는 의미인 0x0D를 송신한다.

- Serial_Init
 UCSRB를 통해 수신과 송신이 가능하게 만들어놓고, 수신 interrupt가 가능하도록 설정한다.
 입력된 bps 값으로 clock을 설정한다

- Serial_PutChar
 UCSRA(상태 레지스터 A)에서 UDR(송신 버퍼)가 비어있는지를 busy-waiting을 통해 체크한다.
 해당 버퍼가 비게 되면 UDR에 데이터를 삽입하여 송신을 시작한다.
```
***
### Difficulties

**최인혁**

> 키보드와 컴퓨터 사이의 데이터 통신에 대해 많은 고민이 있었습니다. 
USB와 블루투스로 통신을 한다는 점 등 데이터 전송 속도에 대해 제한된 환경에서 최대한 키보드가 Real-time으로 동작해야 합니다.
그리고 단순 키보드 입력 뿐만 아니라 페어링 여부, CAPSLOCK 입력 여부 통지 등에 대해서도 추가적으로 통신 및 처리가 가능해야 합니다.
위를 모두 만족시키기 위해 패킷의 크기와 내용을 정교하게 구성하고 하드웨어와 소프트웨어간의 적절한 설정값을 찾기 위해 많은 노력을 기울였습니다. 
이를 통해 단순히 키보드를 컴퓨터에 연결시켜 사용하는 것에도 다양한 issue가 존재할 수 있다는 것을 알게 되었고, 하드웨어와 소프트웨어의 조화가 중요하다는 점을 깨닫게 되었습니다.

**박성우**

> 디바이스 드라이버라는 분야 자체가 사람들이 잘 안하는 분야이다 보니, 관련한 설명이 너무 적어서 난감한 점이 없지 않아 있었습니다. 직접 드라이버 코드를 이해하기 위해 리눅스 커널 코드를 내려받아 하나하나 트레이싱하며 쫓아가면서 드라이버를 이해하다 보니 어느새 I/O 시스템의 흐름을 이해하고 파악할 수 있게 되어 제가 임베디드 개발자로서 한 발자국 나아갈 수 있었던 프로젝트였습니다.

**우인혜**

> 애초에 포인터가 넘 약해서 애초에 c 보는게 많이 힘들었습니다. 그리고 코드가 너무 방대해서 읽기가 힘들었고 모듈로 다 나뉘어 있어서 계속 옮겨가며 봐야 했던게 제일 힘들었습니다.

**이승진**

> 만들기 어려웠던거라 우선 구조 이해하는거부터 한참 걸렸던거같은데 이해하다가 너무 오래걸릴꺼같아서 그냥 무작정 시작했던거 같습니다. 명확하게 이해시켜주는 자료도 많이 없어 어려움이 많았습니다.
***
### License

> GPL
