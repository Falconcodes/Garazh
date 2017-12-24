/* garazh.c
 * Created: 17.12.2017 22:33:00
 * Author: Falcon
 */

#include <mega328p.h>
#include <delay.h>
#include <stdio.h>
#include <string.h>

// Declare your global variables here

#define DATA_REGISTER_EMPTY (1<<UDRE0)
#define RX_COMPLETE (1<<RXC0)
#define FRAMING_ERROR (1<<FE0)
#define PARITY_ERROR (1<<UPE0)
#define DATA_OVERRUN (1<<DOR0)
#define LED PORTB.5

// USART Receiver buffer
#define IN_BUF_SIZE 1024
#define IN_BUF_MASK (IN_BUF_SIZE - 1) //��� ��������� ������ ��������, ���� ��� ���������� ��������� �� IN_BUF_SIZE


volatile char inbuf[IN_BUF_SIZE]="$"; //inner buffer of USART
volatile char com_string[128]="\0"; //� ���� ������ ����� ����������� ������� � ���� �������� "RING", "ERROR" � �.�.
volatile unsigned int cstring_ind=0; //������ �������� � ������� com_string[]
volatile unsigned int rxind_out=0, rxind_in=0, mess = 0;
volatile unsigned int com_detect=0;  //���� ����� �������� ������������ �������
bit i_call=0; //����, ������� ���� �� ������ ���� ����� � ������ �� ���� �������

//#define TIMEOUT 100     //�� ������ ���� ������� ��� � �� �������

// USART ���������� �� ������ 1 �����
interrupt [USART_RXC] void usart_rx_isr(void)
{
char status, data;
status=UCSR0A;
data=UDR0;
if ((status & (FRAMING_ERROR | PARITY_ERROR | DATA_OVERRUN))==0)
   {if (data == 0x0D)        //������� ����� ������� - <enter>
                {
                mess++; //one more message
                inbuf[rxind_in++] = '$'; //��������� ����������� � �����
                rxind_in &= IN_BUF_MASK;
                }
    else 
                {
                if (data != 0x0A) //������� ����������� ������� (LF) � ������
                        {
                        inbuf[rxind_in++] = data;   //���������� � �����
                        rxind_in &= IN_BUF_MASK;
                        }
                }
   }
}

//������� �������������� �������� ������� � �������� ����
void code_com (unsigned int count) {
  //������� ��������� ������      
        if ( strstr(com_string, "OK") )      com_detect = 1; //���� � ������ ��������� ������ ������ �����, ���� ���� - ���������� ��� ��������������� �������
  else  if ( strstr(com_string, "RING") )    com_detect = 2;
  else  if ( strstr(com_string, "ERROR") )   com_detect = 3;
  else  if ( strstr(com_string, "ATI") )     com_detect = 4;
  else  com_detect = 0;
  //�������������� ���� �� ������
        if ( strstr(com_string, "9258807739") || strstr(com_string, "9377167723") )     i_call = 1; //���� ������ �� ����

         

// ������� ��������� ����� switch-case
/*//        switch (com_detect)
//                {
//                case (0x12): if (count == 4) com_detect = 2; break; //R^I^N^G = 0x12
//                case (0x58): if (count == 5) com_detect = 3; break; //ERROR
//                case (0x04): if (count == 2) com_detect = 1; break; //OK
//                case (0x5C): if (count == 3) com_detect = 4; break; //ATI
//                default: com_detect = 0;
//                } */
  }

//������� ��������� ��������  � ����� RX ����
void rx_check_in (void)
    {
    unsigned int count=0;
    com_detect = 0;  //��������� ������� (����� �� ����� ���������� �����)
    com_string [0] = '\0'; //������� ������ �� ������� �������� �������, ��������� � ������ ������ ����� ������   
    cstring_ind = 0;       //� �������� ������ ��� ��������
    
    while (1)
        {
        if (inbuf[rxind_out] != '$') //���� �� ��������� ����� ������� - ������ ������� (�����������)
                {
                com_string [cstring_ind++] = inbuf[rxind_out]; //��������� � ������ ��������� ������� �������� �������
                com_detect ^=  inbuf[rxind_out++]; //������ XOR ���������� �������� � �������������� ������ ������ ������
                rxind_out &= IN_BUF_MASK;
                count++;  //�������, ������� �������� � �������
                }
        else 
                {
                com_string [cstring_ind++] = '\0'; //����� ����� ������ � ������ �������� �������
                rxind_out++;
                rxind_out &= IN_BUF_MASK;
                code_com (count);           //!! ������ ����� - ������������� ������� 
                printf("%s\r", com_string); //��������� � �������� ���������� ������, ����� ���������� �� ��
                break;
                }
        }
    }

unsigned char i;

void main(void)
{
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

printf("AT\r");

delay_ms(200);

printf("ATE0\r"); // ��������� ��� � UART, ����� �� �������� ��� �������� ����� ������ �����������

delay_ms(200);

printf("AT+CPAS\r\n");

/////// ������ �� ����� 
//printf("ATD89258807739;\r");

////// ��� �� �����
//delay_ms(500);
//printf("AT+CMGS=\"89258807739\"\r");
//delay_ms(100);
//printf("TEST\x1A");


// Global enable interrupts
#asm("sei")

while(1) {
  
  if (mess != 0) { //if we have mess in buffer
    mess--;   //minus one
    rx_check_in ();
    //result = strstr(com_string, "RING");
    
    if (com_detect == 2 ) 
      {
      LED=1;
      delay_ms(500);
      LED=0;
      delay_ms(100);
      }
    
    
    
    com_detect = 0;
    i_call = 0;
	}
	
  delay_ms(30);
  PORTB.5=1;
  delay_ms(1);
  PORTB.5=0;
}

   
}