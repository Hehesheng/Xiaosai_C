#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "adc.h"
#include "dac.h"
#include "gpio.h"
#include "TIM.h"

#define com_end printf("\xff\xff\xff")

float xw_save[Need_Num];

int main(void)
{ 
	float freq = 0,zkb = 0;
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);//设置系统中断优先级分组2
	delay_init(168);      //初始化延时函数
	uart_init(115200);		//初始化串口波特率为115200
	
	GPIO_ALLInit();//IO口初始化
	printf("tm0.en=0");
	com_end;
	/*
	先用 TIM2 输入捕获的方式测量频率，若发现为高频信号，转为 TIM2 外部时钟计数模式并开启 TIM3
	*/
	//TIM3_Int_Init(10000-1,8400-1);	//定时器时钟84M，分频系数8400，所以84M/8400=10Khz的计数频率，计数10000次为1000ms     
	//TIM2_Counter_Init();//TIM外部计数
	TIM2_CH1_Cap_Init(0xffffffff,0);//定时器2捕获输入，分频1，84M时钟
	
	while(1)
	{
		if(FINISH && selet_time)//完成计算
		{
			switch(mode_flag)//低频模式
			{
				case Cal_Low:
					freq = 84000000.0/rising_second;//计算频率	
					if(freq < 10000)//判断是否为高频<10kHz
					{
						printf("result.txt=\"%.4f\"",freq);
						com_end;
						delay_ms(500);
					}
					else
					{
						mode_flag = Cal_Hig;//切换为高频
						ALL_UsedTIM_DEInit();
						printf("dw.txt=\"KHz\"");//单位改变
						com_end;
						TIM3_Int_Init(10000-1,840-1);	//定时器时钟84M，分频系数840，所以84M/840=100Khz的计数频率，计数10000次为100ms     
						TIM2_Counter_Init();//TIM外部计数
						//高速初始化
					}
				break;
				case Cal_Hig://高频模式
					if(CLK_NUM >= 1000)//判断是否为低频<10kHz
					{
						freq = CLK_NUM*0.01;
						printf("result.txt=\"%.3f\"",freq);
						com_end;
						delay_ms(400);
					}
					else
					{
						mode_flag = Cal_Low;//切换为低频
						ALL_UsedTIM_DEInit();
						printf("dw.txt=\"Hz\"");//单位改变
						com_end;
						TIM2_CH1_Cap_Init(0xffffffff,0);//定时器2捕获输入，分频1，84M时钟
						//低速初始化
					}
				break;
				case Cal_Zkb://占空比模式
					//freq = 1/((float)rising_second/8400000);
					//k = freq/1000*0.253;
					zkb = (float)falling/(float)rising_second*100;
					printf("result.txt=\"%.2f\"",zkb);
					com_end;
					delay_ms(400);
				break;
				case Cal_Cxw://测量相位模式

				break;
			}
			FINISH = 0;
		}	
		if(selet_time == 0)//切换模式，需要重置
		{
			ALL_UsedTIM_DEInit();//重启
			switch(mode_flag)
			{
				case Cal_Low:
				case Cal_Zkb:
					TIM2_CH1_Cap_Init(0xffffffff,0);//定时器2捕获输入，分频1，84M时钟
				break;
				case Cal_Cxw:
					
				break;
			}
			selet_time = 1;
		}
	}
}


