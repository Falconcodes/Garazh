/* garazh.c
 * Created: 17.12.2017 22:33:00
 * Author: Falcon
 */

#include <mega328p.h>
#include <delay.h>
#include <stdio.h>
#include <string.h>

// Declare your global variables here

#define MY_NUMBERS (strstr(com_string, "9258807739") || strstr(com_string, "9377167723"))

#define DATA_REGISTER_EMPTY (1<<UDRE0)
#define RX_COMPLETE (1<<RXC0)
#define FRAMING_ERROR (1<<FE0)
#define PARITY_ERROR (1<<UPE0)
#define DATA_OVERRUN (1<<DOR0)
#define LED PORTB.5

#define LED_OFF PORTB.2=PORTB.3=PORTB.4=1
#define RED     LED_OFF; PORTB.2=0
#define GREEN   LED_OFF; PORTB.4=0
#define BLUE    LED_OFF; PORTB.3=0

// USART Receiver buffer
#define IN_BUF_SIZE 1024
#define IN_BUF_MASK (IN_BUF_SIZE - 1) //для обнуления номера элемента, если при инкременте перевалил за IN_BUF_SIZE

volatile char inbuf[IN_BUF_SIZE]="$"; //inner buffer of USART
volatile char com_string[128]="\0"; //в этот массив будет сохраняться команда в виде например "RING", "ERROR" и т.п.
volatile unsigned int cstring_ind=0; //индекс элемента в массиве com_string[]
volatile char sms_str[64]="\0"; //в этот массив будет сохраняться команда в виде например "RING", "ERROR" и т.п.
volatile unsigned int smstr_ind=0; //индекс элемента в массиве sms_str[]
volatile unsigned int rxind_out=0, rxind_in=0, mess = 0;
volatile unsigned int com_detect=0;  //сюда будет записана обнаруженная команда
bit i_call=0; //флаг, активен если на модуль идет вызов с одного из моих номеров

//#define TIMEOUT 100     //на случай если команда так и не принята

// USART Прерывание по приему 1 байта
interrupt [USART_RXC] void usart_rx_isr(void)
{
char status, data;
status=UCSR0A;
data=UDR0;
if ((status & (FRAMING_ERROR | PARITY_ERROR | DATA_OVERRUN))==0)
   {if (data == 0x0D)        //получен конец команды - <enter>
                {
                mess++; //one more message
                inbuf[rxind_in++] = '$'; //вставляем разделитель в буфер
                rxind_in &= IN_BUF_MASK;
                }
    else 
                {
                if (data != 0x0A) //очистка непонятного символа (LF) с модуля
                        {
                        inbuf[rxind_in++] = data;   //записываем в буфер
                        rxind_in &= IN_BUF_MASK;
                        }
                }
   }
}

//функция чтения текста принятого СМС, используется в следующей функции code_com();
void sms_read()
  {
    //unsigned int count=0;
    //com_detect = 0;  //обнуление команды (чтобы не мешал предыдущий мусор)
    sms_str [0] = '\0'; //очищаем массив от прошлой принятой команды, записывая в начало символ конца строки   
    smstr_ind = 0;       //и обнуляем индекс его элемента
    
    while (1)
        {
        if (inbuf[rxind_out] != '$') //пока не обнаружен конец команды $ (разделитель)
                {
                sms_str [smstr_ind++] = inbuf[rxind_out++]; //вписываем в массив очередной элемент принятой команды
                rxind_out &= IN_BUF_MASK;
                }
        else 
                {
                sms_str [smstr_ind++] = '\0'; //пишем символ 'конец строки' в строку принятой команды
                rxind_out++;
                rxind_out &= IN_BUF_MASK;
                break;
                }
        }
  mess--;
  }

//функция преобразования полезных ответов в цифровые коды, используется в следующей функции rx_check_in();
void code_com () {
  //сигналы состояния модуля      
        if ( strstr(com_string, "OK") )      com_detect = 1; //ищем в строке принятого ответа нужное слово, если есть - распознаем как соответствующую команду
  else  if ( strstr(com_string, "RING") )    com_detect = 2;
  else  if ( strstr(com_string, "ERROR") )   com_detect = 3;
  else  if ( strstr(com_string, "ATI") )     com_detect = 4;
  else  com_detect = 0;
  
  //доплонительная инфо от модуля
        if ( strstr(com_string, "+CLIP:") && MY_NUMBERS )    i_call = 1; //если идет звонок, и он от одного из заданных номеров               
        else i_call = 0; 
        
        if ( strstr(com_string, "+CMTI:") ) { //если пришло новое СМС
          //отправляем команду чтения нового СМС, затем возвращаемся в осн. цикл и ждем когда от модуля придет "+CMGR:"
          printf("AT+CMGR=1,0\r");
        }
        //else new_sms = 0;
        
        if ( strstr(com_string, "+CMGR:") && MY_NUMBERS ) {  //если модуль выдал содержимое сообщения и оно от номеров из белого списка
          //пропускаем первую строку (она вида +CMGR: "REC UNREAD","+79258807739","","17/12/24,23:32:21+12")
          //следующая строка - сам текст принятого СМС
          sms_read();
          if ( strstr(sms_str, "Test") )    com_detect = 20;
          if ( strstr(sms_str, "Red") )     com_detect = 21;
          if ( strstr(sms_str, "Green") )   com_detect = 22;
          if ( strstr(sms_str, "Blue") )    com_detect = 23;
          
//          if ( strstr(sms_str, "TEST") ) com_detect = 20;
//          if ( strstr(sms_str, "TEST") ) com_detect = 20;
//          if ( strstr(sms_str, "TEST") ) com_detect = 20;
          
          printf("SMS_TEXT: \"%s\"\r", sms_str);
          sms_str [0] = '\0';
          printf("AT+CMGD=1,4\r"); //очистка СМС
          delay_ms(200);  
        } 

// вариант обработки через switch-case
/*//        switch (com_detect)
//                {
//                case (0x12): if (count == 4) com_detect = 2; break; //R^I^N^G = 0x12
//                case (0x58): if (count == 5) com_detect = 3; break; //ERROR
//                case (0x04): if (count == 2) com_detect = 1; break; //OK
//                case (0x5C): if (count == 3) com_detect = 4; break; //ATI
//                default: com_detect = 0;
//                } */
  }

//функция обработки принятой  в буфер RX инфы
void rx_check_in (void)
    {
    //unsigned int count=0;
    com_detect = 0;  //обнуление команды (чтобы не мешал предыдущий мусор)
    com_string [0] = '\0'; //очищаем массив от прошлой принятой команды, записывая в начало символ конца строки   
    cstring_ind = 0;       //и обнуляем индекс его элемента
    
    while (1)
        {
        if (inbuf[rxind_out] != '$') //пока не обнаружен конец команды $ (разделитель)
                {
                com_string [cstring_ind++] = inbuf[rxind_out++]; //вписываем в массив очередной элемент принятой команды
                //com_detect ^=  inbuf[rxind_out++]; //!!!при расскомменте убрать строкой выше "++" после "rxind_out". Делаем XOR полученным символам и инкрементируем индекс буфера приема
                rxind_out &= IN_BUF_MASK;
                //count++;  //считаем, сколько символов в команде
                }
        else 
                {
                com_string [cstring_ind++] = '\0'; //пишем символ 'конец строки' в строку принятой команды
                rxind_out++;
                rxind_out &= IN_BUF_MASK;
                code_com ();           //!! важная часть - раскодировать команду 
                printf("=====%s\r", com_string); //переслать в терминал полученную строку, чтобы посмотреть на ПК
                break;
                }
        }
    }


unsigned char i;

void main(void)
{
DDRB.1=DDRB.2=DDRB.3=DDRB.4=DDRB.5=1;

PORTB.1=1;

LED_OFF;
                                          

// USART 8 Data, 1 Stop, No Parity 9600
UCSR0B=(1<<RXCIE0) | (1<<RXEN0) | (1<<TXEN0);
UCSR0C=(1<<UCSZ01) | (1<<UCSZ00);
UBRR0L=0x67;

for (i=0; i<2; i++){
PORTB.5=1;
delay_ms(100);
PORTB.5=0;
delay_ms(100);
}

// Global enable interrupts
#asm("sei")

printf("AT\r");
delay_ms(100);

printf("ATE0\r"); // Отключаем эхо в UART, чтобы не забивать наш приемный буфер лишней информацией
delay_ms(100);

//printf("AT+CPAS\r\n");
//delay_ms(200);

//очистка старых СМС
printf("AT+CMGD=1,4\r");
delay_ms(100);

/////// звонок на номер 
//printf("ATD89258807739;\r");

////// СМС на номер
//delay_ms(500);
//printf("AT+CMGS=\"89258807739\"\r");
//delay_ms(100);
//printf("TEST\x1A");

RED;
delay_ms(200);
GREEN;
delay_ms(200);
BLUE;
delay_ms(200);
LED_OFF;

while(1) {
  
  if (mess != 0) { //if we have mess in buffer
    mess--; 
    rx_check_in ();
    
    if (com_detect == 2 ) 
      {
      LED=1;
      delay_ms(500);
      LED=0;
      delay_ms(100);
      LED_OFF;
      }
    
    if (com_detect == 21 ) {
      RED;
      delay_ms(500);
      //LED_OFF;
    }
    
    if (com_detect == 22 ) {
      GREEN;
      delay_ms(500);
      //LED_OFF;
    }
    
    if (com_detect == 23 ) {
      BLUE;
      delay_ms(500);
      //LED_OFF;
    }
    
    com_detect = 0;
    
	}
	
  delay_ms(30);
  PORTB.5=1;
  delay_ms(1);
  PORTB.5=0;
}

   
}