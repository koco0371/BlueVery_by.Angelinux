/******************************************************************************
 *                                                                            *
 * 프로젝트명: Angelinux                                                      *
 * 제품명: Bluevery                                                           *
 * 만든 사람들: 최인혁, 박성우, 우인혜, 이승진                                *
 * 개발 기간: 2019.08.13~2019.09.08                                           *
 * 구현 언어: C                                                               *
 * 구현 환경: Linux                                                           *
 *                                                                            *
 ******************************************************************************/

#include <linux/kernel.h> //KERN_INFO와 같은 매크로 사용하기 위해
#include <linux/init.h> //__init __exit 매크로 사용하기 위해
#include <linux/slab.h> //메모리 할당용
#include <linux/usb.h> //usb_device 관련 함수 사용하기 위해
#include <linux/errno.h> //각종 에러 발생하기 위해
#include <linux/module.h> //module함수들 사용하기 위해
#include <linux/spinlock.h> //스핀락 사용
#include <linux/usb/input.h>	//input 이벤트 처리하기 위해

#define BT_keyboard_VENDOR_ID		0x16c0 //usb 디바이스의 vid
#define BT_keyboard_PRODUCT_ID		0x05dc //usb 디바이스의 pid
#define BT_keyboard_MANUFACTURER 	"Angelinux" //usb 디바이스의 제조자 이름
#define BT_keyboard_PRODUCT 		"Bluevery" //usb 디바이스의 제품 이름

/*제어파이프를 통해 pc->usb에 전달하는 bRequest macro*/
#define BT_BOOT_INIT 1 //정상적으로 켜졌는지 확인하기 위해 
#define KEY_FN_CD 0x14
#define KEY_FN_LS 0x24
#define KEY_FN_PS 0x74
#define KEY_FN_EXIT 0x84

/*usb_host_driver에 해당 id를 등록하기 위해 usb_device_id 구조체 변수 선언
 -> 이를 바탕으로 디바이스가 이 드라이버 모듈을 찾음*/
static const struct usb_device_id id_table[] = {
	{USB_DEVICE(BT_keyboard_VENDOR_ID,BT_keyboard_PRODUCT_ID)},
	{},
};

MODULE_DEVICE_TABLE(usb, id_table); //usb host driver에 id_table 등록

static const unsigned char bt_kbd_keycode[256] = {

	
	KEY_GRAVE, KEY_TAB, KEY_CAPSLOCK, KEY_LEFTSHIFT, KEY_LEFTCTRL, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	KEY_1, KEY_Q, KEY_A, KEY_Z, KEY_FN_CD, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	KEY_2, KEY_W, KEY_S, KEY_X, KEY_FN_LS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	KEY_3, KEY_E, KEY_D, KEY_C, KEY_LEFTALT, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	KEY_4, KEY_R, KEY_F, KEY_V, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	KEY_5, KEY_T, KEY_G, KEY_B,KEY_SPACE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	KEY_6, KEY_Y, KEY_H, KEY_N, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	KEY_7, KEY_U, KEY_J, KEY_M, KEY_FN_PS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	KEY_8, KEY_I, KEY_K, KEY_COMMA, KEY_FN_EXIT, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	KEY_9, KEY_O, KEY_L, KEY_DOT, KEY_LEFT, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	KEY_0, KEY_P, KEY_SEMICOLON, KEY_SLASH, KEY_DOWN, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	KEY_MINUS, KEY_LEFTBRACE, KEY_APOSTROPHE, 0, KEY_UP, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	KEY_EQUAL, KEY_RIGHTBRACE, KEY_DELETE, KEY_INSERT, KEY_ESC, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	KEY_BACKSPACE, KEY_BACKSLASH, KEY_ENTER, KEY_RIGHTSHIFT, KEY_RIGHT, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	KEY_LEFTSHIFT, KEY_LEFTCTRL, KEY_LEFTALT, KEY_RIGHTSHIFT, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0

};

/*usb_client_driver에서 다루게될 중심 구조체*/
struct bt_keyboard{ 
	unsigned char 		name[128];
	struct input_dev 	*idev; //input driver event 처리
	struct usb_device 	*udev; //물리적인 디바이스의 정보를 담고 있는 usb_device 구조체
	struct urb 		*led_urb, *int_urb; //디바이스와 통신을 위해 필요한 urb 구조체(ctrl_urb = 제어파이프_OUT_용, int_urb = 인터럽트파이프_IN_용)
	struct usb_ctrlrequest 	*ctrl_req; //제어파이프에 보낼 request 형식 구조체
	unsigned char		buf[5]; //제어파이프에서 request의 답으로 받은 데이터를 담을 버퍼
	char			phys[64];	//물리적인 위치 저장
	unsigned char 		*int_in_buf; //인터럽트파이프로 입력받을 데이터를 담을 버퍼
	unsigned char		old[8];
	size_t 			int_in_len; //인터럽트파이프태의 버퍼 크기
	__u8			int_in_addr; //인터럽트파이프_IN의 종단점 주소
	unsigned char 		newleds;	//새로운 led 상태 등록
	unsigned char		*leds;		// led의 현재 상태가 등록되있는 버퍼(dma용)
	__u32			timeout; //usb_msg함수 타임아웃 시간
	__u32			snd_ctrl_pipe;	//send control pipe
	__u32			rcv_ctrl_pipe;	//receive control pipe

	spinlock_t		lock; //control pipe로 데이터 주고받을때 스핀락하기 위해
	dma_addr_t		int_dma; //interrupt pipe의 dma 구조체
	spinlock_t		led_lock; //스핀락을 위한 변수
	dma_addr_t		led_dma; //control pipe의 dma 구조체

	bool 			led_urb_submitted;	// led urb가 사용되고있는 지를 나타내는 변수

};
void bt_kbd_input_key(struct bt_keyboard *bt_kbd){

	int i;
	for(i=0; i<4;i++) {
		input_report_key(bt_kbd->idev, bt_kbd_keycode[i+224], (bt_kbd->int_in_buf[0] >> i) &1);
			
		if((bt_kbd->int_in_buf[0]>>i) & 1)
			printk(KERN_ALERT "key pressed is %d", bt_kbd_keycode[i+224]);
	}

	for(i=2; i<4; i++){
		//지난 주기에 눌렸던 키가 이번 주기에 눌리지 않았을 경우 떼어졌다는 처리
		if( bt_kbd->old[i]<0xEE && memscan(bt_kbd->int_in_buf+2, bt_kbd->old[i],2)==bt_kbd->int_in_buf+4) {
			if(bt_kbd_keycode[bt_kbd->old[i]]) {
				input_report_key(bt_kbd->idev, bt_kbd_keycode[bt_kbd->old[i]],0);
				printk(KERN_ALERT "old key released is %x", bt_kbd_keycode[bt_kbd->old[i]]);
			}
		}
		//이번 주기에 눌린 키에 대한 처리, 지난 주기에 눌렸던 키는 다시 처리하지 않는다.
		if( bt_kbd->int_in_buf[i] <0xEE && memscan(bt_kbd->old+2, bt_kbd->int_in_buf[i], 2) == bt_kbd->old+4){
			switch(bt_kbd->int_in_buf[i]){
				case KEY_FN_CD:
					if(i==3){
						 input_report_key(bt_kbd->idev,bt_kbd_keycode[bt_kbd->int_in_buf[2]],0);
					}
					input_report_key(bt_kbd->idev, KEY_C, 1);
					input_report_key(bt_kbd->idev, KEY_D, 1);
					input_report_key(bt_kbd->idev, KEY_SPACE, 1);
					input_report_key(bt_kbd->idev, KEY_DOT, 1);
					input_sync(bt_kbd->idev);
					input_report_key(bt_kbd->idev, KEY_DOT, 0);
					input_sync(bt_kbd->idev);
					input_report_key(bt_kbd->idev, KEY_DOT, 1);
					input_report_key(bt_kbd->idev, KEY_ENTER, 1);
					input_sync(bt_kbd->idev);
					input_report_key(bt_kbd->idev, KEY_C, 0);
					input_report_key(bt_kbd->idev, KEY_D, 0);
					input_report_key(bt_kbd->idev, KEY_SPACE, 0);
					input_report_key(bt_kbd->idev, KEY_DOT, 0);
					input_report_key(bt_kbd->idev, KEY_ENTER, 0);
					input_sync(bt_kbd->idev);
					break;
				case KEY_FN_LS:
					if(i==3){
						 input_report_key(bt_kbd->idev,bt_kbd_keycode[bt_kbd->int_in_buf[2]],0);
					}
					input_report_key(bt_kbd->idev, KEY_L, 1);
					input_report_key(bt_kbd->idev, KEY_S, 1);
					input_report_key(bt_kbd->idev, KEY_SPACE, 1);
					input_report_key(bt_kbd->idev, KEY_MINUS, 1);
					input_sync(bt_kbd->idev);
					input_report_key(bt_kbd->idev, KEY_L, 0);
					input_sync(bt_kbd->idev);
					input_report_key(bt_kbd->idev, KEY_L, 1);
					input_report_key(bt_kbd->idev, KEY_ENTER, 1);
					input_sync(bt_kbd->idev);
					input_report_key(bt_kbd->idev, KEY_L, 0);
					input_report_key(bt_kbd->idev, KEY_S, 0);
					input_report_key(bt_kbd->idev, KEY_SPACE, 0);
					input_report_key(bt_kbd->idev, KEY_MINUS, 0);
					input_report_key(bt_kbd->idev, KEY_ENTER, 0);
					input_sync(bt_kbd->idev);
					break;
				case KEY_FN_PS:
					if(i==3){
						 input_report_key(bt_kbd->idev,bt_kbd_keycode[bt_kbd->int_in_buf[2]],0);
					}
					input_report_key(bt_kbd->idev, KEY_P, 1);
					input_report_key(bt_kbd->idev, KEY_S, 1);
					input_report_key(bt_kbd->idev, KEY_ENTER, 1);
					input_sync(bt_kbd->idev);
					input_report_key(bt_kbd->idev, KEY_P, 0);
					input_report_key(bt_kbd->idev, KEY_S, 0);
					input_report_key(bt_kbd->idev, KEY_ENTER, 0);
					input_sync(bt_kbd->idev);
					break;
				case KEY_FN_EXIT:
					if(i==3){
						 input_report_key(bt_kbd->idev,bt_kbd_keycode[bt_kbd->int_in_buf[2]],0);
					}
					input_report_key(bt_kbd->idev, KEY_E, 1);
					input_report_key(bt_kbd->idev, KEY_X, 1);
					input_report_key(bt_kbd->idev, KEY_I, 1);
					input_report_key(bt_kbd->idev, KEY_T, 1);
					input_report_key(bt_kbd->idev, KEY_ENTER, 1);
					input_sync(bt_kbd->idev);
					input_report_key(bt_kbd->idev, KEY_E, 0);
					input_report_key(bt_kbd->idev, KEY_X, 0);
					input_report_key(bt_kbd->idev, KEY_I, 0);
					input_report_key(bt_kbd->idev, KEY_T, 0);
					input_report_key(bt_kbd->idev, KEY_ENTER, 0);
					input_sync(bt_kbd->idev);
					break;
				default:
					input_report_key(bt_kbd->idev, bt_kbd_keycode[bt_kbd->int_in_buf[i]], 1);
					printk(KERN_ALERT "new key pressed is %x", bt_kbd_keycode[bt_kbd->int_in_buf[i]]);
					break;
			}	
		}
	}
	//마찬가지로 2개로 줄었을 경우로 반복문 수정
	
	memcpy(bt_kbd->old, bt_kbd->int_in_buf, 8);
	//input_report_key로 쌓아 놓은 input event 를 input_sync를 호출함을 통해서 한 번에 처리해버림
	input_sync(bt_kbd->idev);
	
}
/*caps lock 눌리면-> led input event 발생-> bt_kbd의 event 함수가 call 되면서 불려지는 함수, 들어온 입력(led의 상태)를 키보드에 전달한다.*/
static int bt_kbd_event(struct input_dev *input, unsigned int type, unsigned int code, int value){

	unsigned long flag;	//irq enable 여부 저장하는 변수
	struct bt_keyboard *bt_kbd = input_get_drvdata(input);	//input core에 저장된 input dev에 저장된 bt_keyboard를 로딩하는 함수

	if(type != EV_LED)	//입력된 event가 EV_LED가 아닐경우 실행하지 않는다.
		return -1;	
	spin_lock_irqsave(&bt_kbd->led_lock,flag);	//spinlock
	bt_kbd->newleds = (!!test_bit(LED_CAPSL, input->led));		//capslock에 대한 LED 점등 상태 등록

	if(bt_kbd->led_urb_submitted) {		//led_urb가 submit된 상태일 경우 spinlock 회복하고 바로 종료
		spin_unlock_irqrestore(&bt_kbd->led_lock,flag);
		return 0;
	}

	if(*(bt_kbd->leds) == bt_kbd->newleds) {	//input core에서 받은 led bit에 대한 정보가 CPU 쪽의 led DMA buffer의 정보와 같은지 체크하고 같으면 데이터를 보낼 필요가 없으므로 spinlock 회복 후 종료
		spin_unlock_irqrestore(&bt_kbd->led_lock,flag);
		return 0;
	}

	*(bt_kbd->leds) = bt_kbd->newleds;	//input core에서 받은 led bit에 대한 정보를 CPU 쪽의 led DMA buffer의 정보를 전달 

	bt_kbd->led_urb->dev = bt_kbd->udev;	//urb 재설정
	if(usb_submit_urb(bt_kbd->led_urb, GFP_ATOMIC))	//urb 제출(데이터 전송 to usb)
		pr_err("usb_submit_urb(leds) failed\n");
	else
		bt_kbd->led_urb_submitted = true;	//성공

	spin_unlock_irqrestore(&bt_kbd->led_lock,flag);	//spinlock 회복하고 종료
	return 0;
};
/*led의 control pipe를 위한 callback 함수(종료될 떄 불려지는 함수)->제대로 전송이 되었는지 다시 한 번 체크하는 함수*/
static void bt_keyboard_led(struct urb *urb)
{
	unsigned long flag;
	struct bt_keyboard *bt_kbd = urb->context;	
	spin_lock_irqsave(&bt_kbd->led_lock,flag);

	if(*(bt_kbd->leds) == bt_kbd->newleds) {	//전송이 제대로 되었으면 bool 변수를 false로 바꾸고 spinlock을 해제 
		bt_kbd->led_urb_submitted = false;
		spin_unlock_irqrestore(&bt_kbd->led_lock,flag);
		return;
	}

	*(bt_kbd->leds) = bt_kbd->newleds;	//전송이 제대로 되지 않았을 경우 다시 한 번 urb를 작성해서 전송

	bt_kbd->led_urb->dev = bt_kbd->udev;
	if(usb_submit_urb(bt_kbd->led_urb, GFP_ATOMIC)) {
		bt_kbd->led_urb_submitted = false;
	}
	spin_unlock_irqrestore(&bt_kbd->led_lock,flag);
}
/*input event의 최초 발생 시, input handler를 input device list에서 찾아 등록하여 open을 콜한다. 이후에는 call 되지 않는다.*/
static int bt_kbd_open(struct input_dev *input){
	struct bt_keyboard *bt_kbd = input_get_drvdata(input);

	bt_kbd->int_urb->dev = bt_kbd->udev;
	if(usb_submit_urb(bt_kbd->int_urb, GFP_KERNEL))
		return -EIO;
	
	return 0;
}
/*input_close_device가 콜되면 close가 콜되며 함수가 실행된다.*/
static void bt_kbd_close(struct input_dev *input){
	struct bt_keyboard *bt_kbd = input_get_drvdata(input);

	usb_kill_urb(bt_kbd->int_urb);
}

/*인터럽트파이프로 데이터가 입력이 완료되면 실행되는 함수
 -> 키입력에 대해서 처리해야 함(아직 구현 x) -> 현재는 그냥 usb디바이스로부터 받은 값에대해 syslog에 출력함 */
static void bt_keyboard_irq(struct urb *urb) {
	struct bt_keyboard *bt_kbd = urb->context; //인터럽트파이프를 통해 데이터를 받은 상황일때를 bt_keyboard 구조체에 대입
	int i;
	unsigned long flag;
	/*urb오류 검사*/
	switch (urb->status) { 
		case 0: //성공
			break;
		case -ECONNRESET:
		case -ENOENT:
		case -ESHUTDOWN:
			return; //위와 같은 에러 있으면 리턴
		default:
			goto resubmit; //다른 에러는 바로 urb 재설정
	}
	switch (bt_kbd->int_in_buf[1]) {
		case 1:	//key 입력시
			bt_kbd_input_key(bt_kbd);	//새로 들어온 key와 이전 주기에 눌린 key set을 비교하여 새로운 입력 이벤트를 실행하는 함수
			break;
		case 2:	//blue tooth 연결시 capslock의 상태를 전달
			printk(KERN_INFO "BLUETOOTH CONNECTED\n");
		
			spin_lock_irqsave(&bt_kbd->led_lock,flag);
			*(bt_kbd->leds) = (!!test_bit(LED_CAPSL, bt_kbd->idev->led));

			bt_kbd->led_urb->dev = bt_kbd->udev;
		
			if(usb_submit_urb(bt_kbd->led_urb, GFP_ATOMIC))
				pr_err("usb_submit_urb(leds) failed\n");
			else
				bt_kbd->led_urb_submitted = true;

			spin_unlock_irqrestore(&bt_kbd->led_lock,flag);
			break;
		case 3: //bluetooth 연길이 해제됨
			printk(KERN_INFO "BLUETOOTH DISCONNECTED\n");
			break;
		case 4://bluetooth(module)에서 보내진 command
			printk(KERN_INFO "OTHER_COMMAND\n");
			break;
		default:
			printk(KERN_INFO "ERROR\n");
			break;
	}

resubmit:
	usb_submit_urb(urb,GFP_ATOMIC); //urb 다시 설정
}
/*필요한 메모리를 할당하는 함수*/
static int bt_keyboard_alloc_mem(struct usb_device *udev, struct bt_keyboard *bt_kbd) {
	if(!(bt_kbd->int_urb = usb_alloc_urb(0,GFP_KERNEL)))
		return -1;
	if(!(bt_kbd->led_urb = usb_alloc_urb(0, GFP_KERNEL)))
		return -1;
	if(!(bt_kbd->int_in_buf = usb_alloc_coherent(udev,8,GFP_ATOMIC,&bt_kbd->int_dma)))
		return -1;
	if(!(bt_kbd->ctrl_req = kmalloc(sizeof(struct usb_ctrlrequest),GFP_KERNEL)))
		return -1;
	if(!(bt_kbd->leds = usb_alloc_coherent(udev,1,GFP_ATOMIC,&bt_kbd->led_dma)))
		return -1;
	return 0;
}
/*할당받은 메모리를 해제하는 함수*/
static void bt_keyboard_free_mem(struct usb_device *udev, struct bt_keyboard *bt_kbd) {
	usb_free_urb(bt_kbd->int_urb);
	usb_free_coherent(udev,8,bt_kbd->int_in_buf,bt_kbd->int_dma);
	usb_free_coherent(udev,1,bt_kbd->leds,bt_kbd->led_dma);
	kfree(bt_kbd->ctrl_req);
	usb_free_urb(bt_kbd->led_urb);
}
/*디바이스가 id_table을 이용해 해당 드라이버 모듈을 찾았을 경우 호출되는 함수
->init 작업이 실행되어야 함*/
static int bt_keyboard_probe(struct usb_interface *interface, const struct usb_device_id *id) {
	
	struct usb_device *udev = interface_to_usbdev(interface); //물리적인 usb device의 정보들을 담고있는 udev 변수, 우변의 함수를 통해 matching이 성공한 usb_device 구조체를 얻어온다.
	struct usb_host_interface *iface_desc; //해당 device에 대한 endpoint들의 정보를 담기 위해 선언
	struct usb_endpoint_descriptor *endpoint; //endpoint의 명세에 대한 정보를 담기 위해 선언
	struct bt_keyboard *bt_kbd; //실질적으로 모듈에서 사용될 구조체 선언
	struct input_dev *input; //input device 명세 작성 및 드라이버 연결을 위해 필요
	int err,i; //err = 에러 값 반환용, i = for문 counting용 
	/*vid,pid 뿐만 아니라, 제조자, 제품명도 일치하는지 판단하기 위해*/
	if(strcmp(udev->manufacturer,BT_keyboard_MANUFACTURER)) //제조자 비교
		return -ENODEV;
	
	if(strcmp(udev->product, BT_keyboard_PRODUCT)) //제품명 비교
		return -ENOMEM;

	input= input_allocate_device(); //input 장치를 위한 공간 할당
	bt_kbd = kzalloc(sizeof(struct bt_keyboard),GFP_KERNEL); //bt_keyboard 구조체 메모리 동적할당 (앞으로 계속 사용하기 위해)
	if(bt_kbd == NULL){
		dev_err(&interface->dev, "kzalloc failed\n");
		err = -ENOMEM;
		goto error;
	} //제대로 할당 되었는지 체크함

	bt_kbd->idev = input;	//dev의 idev 포인터에 input 연결
	bt_kbd->udev = usb_get_dev(udev); //dev의 udev변수(usb_device 구조체)에 usb core에 참조하고 있는 usb_device 개수를 1 증가 시킴.
	iface_desc = interface->cur_altsetting; //현재 altsetting 즉, 해당 디바이스의 endpoint들의 정보 받음
	spin_lock_init(&bt_kbd->lock);	//데이터 접근을 위한 spin_lock  설정
	
	strcpy(bt_kbd->name,"Bluevery");
	input->name = bt_kbd->name;
	//input->name의 이름 완성

	usb_make_path(udev, bt_kbd->phys, sizeof(bt_kbd->phys));	//dev->phy_location에 udev의 물리적인 위치 저장
	strlcat(bt_kbd->phys, "/input0", sizeof(bt_kbd->phys));
	
	input->phys = bt_kbd->phys;	//물리 위치 등록
	usb_to_input_id(udev, &input->id);	//input에 bus type, vid, pid, version을 설정한다.
	input->dev.parent = &interface->dev;	// input의 parent를 iface_desc->dev로 설정한다.

	input_set_drvdata(input, bt_kbd);	//input device의 driver에  keyboard driver 추가

	input->evbit[0] = BIT_MASK(EV_KEY)|BIT_MASK(EV_LED);	//event bit에 key event, led event, auto repeating event 추가
	input->ledbit[0] = BIT_MASK(LED_CAPSL);		//led bit에 capslock led 추가
	for(i = 0; i < 255; i++)		//input core에 keycode 세팅
		set_bit(bt_kbd_keycode[i],input->keybit);
	clear_bit(0,input->keybit);	//0번 비트는 입력 안되게 함
	//keycode setting code 들어갈 부분
	
	input->event = bt_kbd_event;
	input->open = bt_kbd_open;
	input->close = bt_kbd_close;

	/*iface_desc로부터 인터럽트_IN 파이프의 정보를 얻기 위해 endpoint개수만큼 반복*/
	for(i = 0; i < iface_desc->desc.bNumEndpoints; ++i) { 
		endpoint = &iface_desc->endpoint[i].desc; //endpoint 명세 순서대로 하나씩 얻어옴
		/*아직 int_in_addr이 저장된적 없고, 해당 endpoint가 int_in_을 
		위한 것 이 맞으면 인터럽트_IN파이프 init작업 해줌*/
		if(!bt_kbd->int_in_addr && usb_endpoint_is_int_in(endpoint)) { 
			bt_kbd->int_in_len = 
				le16_to_cpu(endpoint->wMaxPacketSize); //파이프 버퍼 길이 설정
			bt_kbd->int_in_addr = endpoint->bEndpointAddress; //종단점 주소 설정
			//버퍼할당은 나중에
			break; //반복문 탈출
		}
	}
	if(!(bt_kbd->int_in_addr)) //인터럽트_IN파이프에 대한 명세가 없었을 경우, error로 go 
		goto error;
	if(bt_keyboard_alloc_mem(udev,bt_kbd)) //각종 메모리 할당하는 함수 호출, 잘못되었을경우 error2로 go
		goto error2;

	usb_set_intfdata(interface,bt_kbd); //interface의 driver_data에 dev 삽입

	spin_lock(&bt_kbd->lock);
	bt_kbd->snd_ctrl_pipe = usb_sndctrlpipe(bt_kbd->udev,0);	//send control pipe 생성
	bt_kbd->rcv_ctrl_pipe = usb_rcvctrlpipe(bt_kbd->udev,0);	//receive control pipe 생성
	err = usb_control_msg(bt_kbd->udev, bt_kbd->snd_ctrl_pipe,
			BT_BOOT_INIT, USB_TYPE_VENDOR | USB_DIR_OUT,
			0, 0,
			0, 0,
			bt_kbd->timeout);	//bluetooth keyboard boot(init)
	err = usb_control_msg(bt_kbd->udev, bt_kbd->rcv_ctrl_pipe,
			BT_BOOT_INIT, USB_TYPE_VENDOR | USB_DIR_IN,
			0, 0,
			bt_kbd->buf,5,
		       	bt_kbd->timeout);	//연결 결과 메시지
	spin_unlock(&bt_kbd->lock);
		
	if(bt_kbd->buf[1] != 1)
	{
		dev_err(&interface->dev, "Bluevery Boot failed\n");
		err = -ENOMEM;
		goto error2;
	}		
	for(i = 0; i<8; i++)	//old buffer 세팅
		bt_kbd->old[i] = 255;

	printk(KERN_INFO "Bluevery is connected\n");
	//usb request block(urb) usb의 request를 처리하는 urb를 작성하는 코드로, interrupt를 받아들이는 파이프에 대한 정보를 채운다.
	//bt_kbd의 int_urb를 작성함, complete 함수를 bt_keyboard_irq로 지정하고, 주기를 endpoint->bInterval로 설정
	usb_fill_int_urb(bt_kbd->int_urb, bt_kbd->udev, usb_rcvintpipe(bt_kbd->udev,bt_kbd->int_in_addr),
			bt_kbd->int_in_buf, bt_kbd->int_in_len,
			bt_keyboard_irq, bt_kbd, endpoint->bInterval);
	
	bt_kbd->int_urb->transfer_dma = bt_kbd->int_dma;	//interrrupt urb의 dma 설정
	bt_kbd->int_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;	//dma가 이미 설정되어 있으니 cpu가 따로 프로그래밍 하지 않도록 함.

	//control request 설정
	bt_kbd->ctrl_req->bRequestType = USB_TYPE_CLASS | USB_RECIP_INTERFACE;	
	bt_kbd->ctrl_req->bRequest = 0x09;
	bt_kbd->ctrl_req->wValue = cpu_to_le16(0x200);
	bt_kbd->ctrl_req->wIndex = cpu_to_le16(iface_desc->desc.bInterfaceNumber);
	bt_kbd->ctrl_req->wLength = cpu_to_le16(1);

	//control urb 설정, complete 함수를 bt_keyboard_led로 설정
	usb_fill_control_urb(bt_kbd->led_urb, bt_kbd->udev, bt_kbd->snd_ctrl_pipe,
			(void *) bt_kbd->ctrl_req, bt_kbd->leds, 1, 
			bt_keyboard_led, bt_kbd);
	bt_kbd->led_urb->transfer_dma = bt_kbd->led_dma;
	bt_kbd->led_urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
	
	err = input_register_device(bt_kbd->idev);	//dev->idev->dev.kobj의 path를 받아오고, name과 path를 출력한 뒤, path의 영역을 free하고, list_add_tail()을 이용해서 idev->node를 input_dev_list의 tail에 추가한다. 그리고 list_for_each_entry()를 실행하여 input_handler_list를 돌면서 dev를 만나면 handler(driver)를 추가한다. 
	if(err)
		goto error2;

	device_set_wakeup_enable(&udev->dev, 1);	
	
	return 0;
error2:
	bt_keyboard_free_mem(udev,bt_kbd);	//input_dev 등록에 실패했으므로 할당했던 메모리를 해제한다.
error:
	input_free_device(input);	//allocation이 실패했으므로 할당해제
	kfree(bt_kbd);
	return err;
}

/*usb device 제거작업 담당(usb가 제거 되었을 때 불려지는 함수)*/
static void bt_keyboard_disconnect(struct usb_interface *interface) {
	struct usb_device *udev = interface_to_usbdev(interface);
	struct bt_keyboard *bt_kbd = usb_get_intfdata(interface);

	usb_set_intfdata(interface,NULL);	//usb core에 등록되어있던 interface를 비움
	if(bt_kbd) {
		usb_kill_urb(bt_kbd->int_urb);	//urb 할당 해제
		input_unregister_device(bt_kbd->idev);	//input_dev 등록 해제
		bt_keyboard_free_mem(udev,bt_kbd);	//메모리 할당해제
		usb_put_dev(udev);	//등록된 usb device 개수를 줄임
		kfree(bt_kbd);	
	}
	printk(KERN_INFO "Bluevery is disconnected");
}

static struct usb_driver bt_keyboard_driver = {
	.name = "Bluevery",
	.probe = bt_keyboard_probe,
	.disconnect = bt_keyboard_disconnect,
	.id_table = id_table,
};

module_usb_driver(bt_keyboard_driver);	//usb_driver 등록

MODULE_AUTHOR("Angelinux");
MODULE_DESCRIPTION("Bluetooth Keyboard for Linux");
MODULE_LICENSE("GPL");

