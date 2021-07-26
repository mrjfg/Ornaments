#include "qi.h"

void Header(void) //前导11到25个1，这里用18个1
{
    uint8 i;
    for (i = 0; i < 18; i++)
    {
        LED3_On();
        delay_us(250);
        LED3_Off();
        delay_us(250);
    }
}
void Code(uint8 aa) //字节编码
{
    uint8 i, j = 0;
    LED3_Toggle(); //起始位
    delay_us(500);
    for (i = 0; i < 8; i++) //数据位
    {
        if (aa & 0x01)
        {
            j += 1;
            LED3_Toggle();
            delay_us(250);
            LED3_Toggle();
            delay_us(250);
        }
        else
        {
            LED3_Toggle();
            delay_us(500);
        }
        aa >>= 1;
    }
    if (j % 2) //检验位
    {
        LED3_Toggle();
        delay_us(500);
    }
    else
    {
        LED3_Toggle();
        delay_us(250);
        LED3_Toggle();
        delay_us(250);
    }

    LED3_Toggle(); //停止位
    delay_us(250);
    LED3_Toggle();
    delay_us(250);
}
void ping(void) //PING包
{
    Header();
    Code(0x01);
    Code(0x80);
    Code(0x01 ^ 0x80);
    LED3_Off();
}
void id(void) //id包
{
    Header();
    Code(0x71);
    Code(0x10);
    Code(0x00);
    Code(0x01);
    Code(0x00);
    Code(0x01);
    Code(0x00);
    Code(0x00);
    Code(0x71 ^ 0x10 ^ 0x00 ^ 0x01 ^ 0x00 ^ 0x01 ^ 0x00 ^ 0x00);
    LED3_Off();
}
void config(void) //配置包
{
    Header();
    Code(0x51);
    Code(0x0a);
    Code(0x00);
    Code(0x00);
    Code(0x00);
    Code(0x00);
    Code(0x51 ^ 0x0a ^ 0x00 ^ 0x00 ^ 0x00 ^ 0x00);
    LED3_Off();
}
void ConErr(void) //误差控制包
{
    Header();
    Code(0x03);
    Code(0x02);
    Code(0x03 ^ 0x02);
    LED3_Off();
}
void RecPWR(void) //接收功率包
{
    Header();
    Code(0x04);
    Code(0xff);
    Code(0x04 ^ 0xff);
    LED3_Off();
}
int main(void)
{
    sysinit();
    SysTick_SetCallBack(SysTick_CallBack);
    delay_us(500);
    LED3_Init();
    ping();
    delay_ms(10);
    id();
    delay_ms(10);
    config();
    delay_ms(50);
    while (1)
    {
        ConErr(); //修改包数据可以改变功率
        delay_ms(500);
    }
}
