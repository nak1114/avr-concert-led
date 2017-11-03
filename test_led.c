#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

typedef unsigned char ui8;
typedef   signed char  i8;
typedef unsigned short ui16;

#if 0
#define CMEM EEMEM
typedef const unsigned char* CPTR;
#define get_byte eeprom_read_byte
#define get_word eeprom_read_word

#else
#define CMEM PROGMEM 
typedef PGM_P CPTR ;
#define get_byte pgm_read_byte_near
#define get_word pgm_read_word_near

#endif

#define KEYF_CHANGE  (0x80)
#define KEYM_WHILE   (0x00)
#define KEYM_TMOUT   (0x08)
#define KEYM_UNTIL   (0x10)
#define KEYM_IMD     (0x04)


#define KEY_A        (0x01)
#define KEY_B        (0x02)
#define KEY_NONE     (KEY_A|KEY_B)
#define KEY_AB       (0x00)
#define MASK_KEY     (KEY_A|KEY_B)

#define CNT_TIMEOUT  ((ui16)0x0300)


#define END_OF_LOOP (0x80)
#define FLG_MAKO    (0x40)
#define LED_RED   (0x08)//GPIO
#define LED_GREEN (0x04)//GPIO
#define LED_BLUE  (0x10)//GPIO
#define MASK_LED (LED_RED|LED_GREEN|LED_BLUE)


ui8 orgbuf[64] EEMEM ={ 
                        0x35,0x00,0x00,0x00,  // #xx0000  //0:haruka(red)
                        0xb6,0x00,0x3c,0x3c,  // #3cff00  //1:miki
                        0x04,0x00,0x00,0x00,  // #xxxx00  //2:amimami
                        0xb5,0x00,0x91,0x91,  // #xx9100  //3:yayoi
                        0x3b,0x00,0x00,0x00,  // #0000xx  //4:chihaya
                        0x00,0x00,0x00,0x00,  // #xxxxxx  //5:yuki
                        0x26,0x00,0x00,0x00,  // #00xx00  //6:ritu
                        0xd9,0xbc,0xbc,0xbc,  // #ff00bc  //7:azu
                        0xd9,0x77,0x77,0x77,  // #ff0077  //8:iori
                        0x3b,0x00,0xef,0xef,  // #00efxx  //9:hibiki
                        0x79,0x1b,0x38,0x66,  // #b51d66  //A:takane
 
                        0x03,0x00,0x00,0x00,  // #xxxxxx  //B:
                        0x03,0x00,0x00,0x00,  // #xxxxxx  //C:
                        0x03,0x00,0x00,0x00,  // #xxxxxx  //D:
                        0x03,0x00,0x00,0x00,  // #xxxxxx  //E:
                        0x03,0x00,0x00,0x00   // #xxxxxx  //F:
                        };                

ui8 ledbit[ 4] CMEM  ={LED_BLUE,LED_RED,LED_GREEN,0x00};

ui8  tbl_color[7];
ui8 *ptr_cur_tbl_color;
ui8 cur_color;
volatile ui8 key_mes;
volatile ui8 now;
ui8 src_buffer[4];

void load_color(void){
	ui8 prev;
	ui8 cur;
	ui8 i;

	ui8 tmp;
	ui8 tmp2;
	ui8 tmp1;
	ui8 *cur_buf;
	ui8 *src_buf;
	ui16 tmpw;

	cur_buf=tbl_color;
	src_buf=src_buffer;

	tmp=*src_buf++;
	prev=0;
	tmp2=(tmp&0x03);//^0x03;
	cur =(tmp2<<2)|0x03;

	for(i=3;i!=0;i--){
		tmp2=*src_buf++;
		if(prev!=tmp2){
			prev=tmp2;
			*cur_buf++=cur ;
			*cur_buf++=tmp2;
		}
		tmp>>=2;tmp1=(tmp&0x03);
		if(tmp1==0x00)break;

		tmpw=get_word(ledbit+tmp1-1);
		tmp2=tmpw&0x00ff;
		tmp1=tmpw>>8;
        cur=(cur & ~tmp1)|tmp2;
	}
	*cur_buf++=cur |END_OF_LOOP;
}
void change_col(ui8 dir){
	ui8*buf;
	ui8 i;
	ui8 col;
	ui8 tmp;

	buf=src_buffer;
	col=cur_color+dir;

	for(i=4;i!=0;){
		col&=0x3f;
		*buf=tmp=eeprom_read_byte(col);
		if(i==4 && (tmp)==0x03){
			col+=dir;
			continue;
		}
		buf++;
		col++;i--;
	}
	cur_color=col-4;
	cli();
	PORTB=0xff;
	load_color();
	ptr_cur_tbl_color=tbl_color;
	OCR0A=0;
	sei();
}



ISR(TIM0_COMPA_vect){
	ui8 tmp;
	ui8 *ptr=ptr_cur_tbl_color;
	tmp =*ptr++;
/*
	if((tmp&FLG_MAKO)){
		mako_cnt++;
		if(mako_cnt&0x80)	tmp=0;
		PORTB=tmp|(PORTB&~MASK_LED);
		return;
	}
*/
	PORTB=tmp;//(PORTB&~MASK_LED);

	if(!(tmp&END_OF_LOOP)){
		OCR0A  =  *ptr++;
		ptr_cur_tbl_color=(ptr);
	}else{
		OCR0A = 0;
		ptr_cur_tbl_color=tbl_color;
		key_mes=(PINB&MASK_KEY);
	}
	return;
}


ui8 getKey(ui8 key){
	ui16 cnt;
	ui8 keyp;
	ui8 keym;
	ui8 ret;
//	set_sleep_mode(SLEEP_MODE_IDLE);
	MCUCR=0x20;
	
	keym=(key&KEYM_UNTIL);               //until:true while:false
	do{
		for(cnt=CNT_TIMEOUT;cnt>=0;cnt--){
			while(((ret=key_mes)&KEYF_CHANGE)!=0){asm("sleep"::);}
			key_mes=KEYF_CHANGE;
			keyp=(ret^(key&(MASK_KEY|KEYM_IMD)));//diff :true same :false
			if(  keyp && !keym ){return ret;}
			if( !keyp &&  keym ){return ret;}
		}
	}while(!(key&KEYM_TMOUT));
	return KEYM_TMOUT|ret;
}

EMPTY_INTERRUPT(INT0_vect);//{return;}

void deep_sleep(void){
//	cli();
//	sei();
//	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	DDRB = 0x00;
    getKey(KEYM_UNTIL|KEY_NONE);
	MCUCR=0x30;
	GIMSK=0x40;
	asm("sleep"::);
	GIMSK=0x00;
	DDRB = 0xfc;
}

void using_seq(ui16 tim){
	switch(tim){
		case 0x0002:change_col( 4);break;
		case 0xfffe:change_col(-4);break;
		case 0x0100:	deep_sleep();	break; //’·‚¨‚µ
		case 0xff00:	deep_sleep();	break; //’·‚¨‚µ
		default:						break;
	}
	return;
}


int main(void)
{
	ui16 key_cnt=0;
	MCUCR=0;
//	PORTB = 0xff;
	DDRB = 0xfc;

//	sleep_enable();

	TIMSK0=0x04;
	TCCR0A=0x00;
	TCCR0B=0x04;//5:1/1024 4:1/256 3:1/64 2:1/8 1:1/1

	cur_color=0;
	change_col(0);

	while(1) {
		ui8 tmp;
/*
		tmp=getKey(KEYM_WHILE|KEY_NONE);
		    getKey(KEYM_TMOUT|KEYM_UNTIL|KEY_NONE);

		if(tmp&KEY_A){change_col( 4);}
		if(tmp&KEY_B){change_col(-4);}
*/

		tmp=getKey(KEYM_IMD);
		if(tmp==KEY_NONE){
			key_cnt=0;
		}else if(tmp==KEY_AB){
//			setting_seq();
		}else{
			key_cnt=(tmp==KEY_A)?(key_cnt+1):(key_cnt-1);
			using_seq(key_cnt);
		}

	}

	return 0;
}
