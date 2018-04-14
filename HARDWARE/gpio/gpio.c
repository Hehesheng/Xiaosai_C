#include "gpio.h"

//---------------------------以下变量分割线----------------------------------------------//

u32 CLK_NUM = 0;//计数
u32	rising_first,rising_second,falling;//两个上升沿 的计数器数据存储以及 下降沿 数据存储
u8 FINISH = 0;//完成标志
u8 selet_time = 1;//标志模式是否切换
u8 rising_flag = 0;//判断第几次捕获中断
u8 mode_flag = Cal_Low;//模式选择（低频模式，高频模式，测量占空比模式）

//---------------------------以上变量分割线----------------------------------------------//

//---------------------------以下中断分割线----------------------------------------------//

//定时器2中断服务函数
void TIM2_IRQHandler(void)
{
	//TIM2->DIER &= (uint16_t)~TIM_IT_CC1;//不允许TIM2中断更新，防止频繁进入中断卡死
	if(FINISH == 0)//捕获1发生捕获事件 且FINISH为0
	{
		switch(mode_flag)
		{
			case Cal_Low://低频模式
				if(rising_flag == 0)//第一次上升沿
				{
					TIM2->CNT = 0;
					rising_flag = 1;
				}
				else if(rising_flag == 1)//第二次上升沿
				{
					rising_second = TIM2->CCR1;
					rising_flag = 0;
					FINISH = 1;
				}
			break;
			case Cal_Zkb://占空比模式
				if(rising_flag == 0)//第一次上升沿
				{
					TIM2->CNT = 0;
					TIM_OC1PolarityConfig(TIM2,TIM_ICPolarity_Falling); //设置下降沿捕获
					rising_flag = 1;
				}
				else if(rising_flag == 1)//第二次下降沿
				{
					falling = TIM2->CCR1;
					TIM_OC1PolarityConfig(TIM2,TIM_ICPolarity_Rising); //设置上升沿捕获
					rising_flag = 2;
				}
				else if(rising_flag == 2)//第三次上升沿
				{
					rising_second = TIM2->CCR1;
					rising_flag = 0;
					FINISH = 1;//标志完成
				}
			break;
			case Cal_Cxw://测量相位模式
				if(rising_flag == 0)
				{
					TIM2->CNT = 0;//计数器清0
//					TIM5->CNT = 0;
					rising_flag = 1;
				}
				else if(rising_flag == 1)
				{
					rising_first = TIM2->CNT;
//					rising_second = TIM5->CNT;
					rising_flag = 0;
					FINISH = 1;
				}
			break;
		}
	}
	TIM_ClearITPendingBit(TIM2, TIM_IT_CC1|TIM_IT_Update); //清除中断标志位
	//TIM2->DIER |= TIM_IT_CC1;//允许TIM2中断更新，防止频繁进入中断卡死
}

//定时器3中断服务函数
void TIM3_IRQHandler(void)
{
	TIM_ClearITPendingBit(TIM3,TIM_IT_Update);  //清除中断标志位
	CLK_NUM = TIM_GetCounter(TIM2);//读取计数
	TIM_SetCounter(TIM2,0);//计数器清0
	FINISH = 1;//完成读取
}

//定时器4中断服务函数
void TIM4_IRQHandler(void)
{
	if(rising_flag == 0)
	{
		TIM2->CNT = 0;//计数器清0
		rising_flag = 1;
	}
	else 
	{
		rising_first = TIM2->CNT;
		rising_flag = 0;
		FINISH = 1;
	}
	TIM_ClearITPendingBit(TIM4, TIM_IT_CC1|TIM_IT_Update); //清除中断标志位
}

//定时器5中断服务函数
void TIM5_IRQHandler(void)
{
	if(rising_flag == 0)
	{
		TIM2->CNT = 0;//计数器清0
//					TIM5->CNT = 0;
		rising_flag = 1;
	}
	else if(rising_flag == 1)
	{
		rising_first = TIM2->CNT;
//					rising_second = TIM5->CNT;
		rising_flag = 0;
		FINISH = 1;
	}
	TIM_ClearITPendingBit(TIM5, TIM_IT_CC1|TIM_IT_Update); //清除中断标志位
}

//---------------------------以上中断分割线----------------------------------------------//


//---------------------------IO口配置---------------------------------------------------//

void GPIO_ALLInit(void)
{    	 
	GPIO_InitTypeDef  GPIO_InitStructure;

  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);//使能GPIOA时钟
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);//使能GPIOD时钟

  //GPIOA15初始化设置
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_15;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;//复用模式
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;//推挽输出
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;//100MHz
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_Init(GPIOA, &GPIO_InitStructure);//初始化
	
	//GPIOB6初始化设置
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;//复用模式
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;//推挽输出
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;//100MHz
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
  GPIO_Init(GPIOD, &GPIO_InitStructure);//初始化
}

//--------------------------------------------------------------------------------------//


//---------------------------以下为低频测频初始化------------------------------------//

//通用定时器TIM2_CH1输入捕获初始化
//arr：自动重装值。
//psc：时钟预分频数
//定时器溢出时间计算方法:Tout=((arr+1)*(psc+1))/Ft us.
//Ft=定时器工作频率,单位:Mhz
void TIM2_CH1_Cap_Init(u32 arr,u16 psc)
{
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	TIM_ICInitTypeDef TIM2_ICInitStructure;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2,ENABLE);          //TIM2时钟使能    

	GPIO_PinAFConfig(GPIOA,GPIO_PinSource15,GPIO_AF_TIM2); //PA15复用位定时器2

	TIM_TimeBaseStructure.TIM_Prescaler=psc;  //定时器分频
	TIM_TimeBaseStructure.TIM_CounterMode=TIM_CounterMode_Up; //向上计数模式
	TIM_TimeBaseStructure.TIM_Period=arr;   //自动重装载值
	TIM_TimeBaseStructure.TIM_ClockDivision=TIM_CKD_DIV1; 

	TIM_TimeBaseInit(TIM2,&TIM_TimeBaseStructure);

	//初始化TIM2输入捕获参数
	TIM2_ICInitStructure.TIM_Channel = TIM_Channel_1; //CC1S=01         选择输入端 IC1映射到TI1上
	TIM2_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;        //上升沿捕获
	TIM2_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI; //映射到TI1上
	TIM2_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;         //配置输入分频,不分频 
	TIM2_ICInitStructure.TIM_ICFilter = 0x0f;//IC1F=0000 配置输入滤波器 不滤波
	TIM_ICInit(TIM2, &TIM2_ICInitStructure);

	TIM_ITConfig(TIM2,TIM_IT_Update|TIM_IT_CC1,ENABLE);//允许更新中断 ,允许CC1IE捕获中断        

	TIM_Cmd(TIM2,ENABLE );         //使能定时器2

	NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=1;//抢占优先级
	NVIC_InitStructure.NVIC_IRQChannelSubPriority =1;                //子优先级
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;                        //IRQ通道使能
	NVIC_Init(&NVIC_InitStructure);        //根据指定的参数初始化VIC寄存器   
}

//----------------------------以上为低频测频初始化-----------------------------------//


//---------------------------以下为高频测频初始化------------------------------------//

//通用定时器TIM2中断初始化
//使用TIM2_ETR的PA15进行外部时钟
//方便计数
void TIM2_Counter_Init(void)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2,ENABLE);  ///使能TIM2时钟
	
  TIM_TimeBaseInitStructure.TIM_Period = 0xffffffff; 	//自动重装载值
	TIM_TimeBaseInitStructure.TIM_Prescaler=0x00;  //定时器分频
	TIM_TimeBaseInitStructure.TIM_CounterMode=TIM_CounterMode_Up; //向上计数模式
	TIM_TimeBaseInitStructure.TIM_ClockDivision=TIM_CKD_DIV1; //分频系数
	
	TIM_TimeBaseInit(TIM2,&TIM_TimeBaseInitStructure);//初始化TIM2
	
	TIM_ITRxExternalClockConfig(TIM2,TIM_TS_ETRF);        //外部触发输入ETRF
	GPIO_PinAFConfig(GPIOA,GPIO_PinSource15,GPIO_AF_TIM2);//复用声明
	
	//TIM_ITConfig(TIM1,TIM_IT_Update,ENABLE); //允许定时器1更新中断
	
	TIM_ETRClockMode1Config(TIM2, TIM_ExtTRGPSC_OFF,TIM_ExtTRGPolarity_NonInverted, 0x00); //使用外部时钟计数
	
	TIM_Cmd(TIM2,ENABLE); //使能定时器1
}

//通用定时器3中断初始化
//arr：自动重装值。
//psc：时钟预分频数
//定时器溢出时间计算方法:Tout=((arr+1)*(psc+1))/Ft us.
//Ft=定时器工作频率,单位:Mhz
//这里使用的是定时器3
void TIM3_Int_Init(u16 arr,u16 psc)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3,ENABLE);  ///使能TIM3时钟
	
  TIM_TimeBaseInitStructure.TIM_Period = arr; 	//自动重装载值
	TIM_TimeBaseInitStructure.TIM_Prescaler=psc;  //定时器分频
	TIM_TimeBaseInitStructure.TIM_CounterMode=TIM_CounterMode_Up; //向上计数模式
	TIM_TimeBaseInitStructure.TIM_ClockDivision=TIM_CKD_DIV1; 
	
	TIM_TimeBaseInit(TIM3,&TIM_TimeBaseInitStructure);//初始化TIM3
	
	TIM_ITConfig(TIM3,TIM_IT_Update,ENABLE); //允许定时器3更新中断
	TIM_Cmd(TIM3,ENABLE); //使能定时器3
	
	NVIC_InitStructure.NVIC_IRQChannel=TIM3_IRQn; //定时器3中断
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=0x01; //抢占优先级1
	NVIC_InitStructure.NVIC_IRQChannelSubPriority=0x01; //子优先级1
	NVIC_InitStructure.NVIC_IRQChannelCmd=ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

//---------------------------以上为高频测频初始化-----------------------------------//

//---------------------------以下为测相位初始化-----------------------------------//

//通用定时器TIM2_CH1_TIM4_CH1输入捕获初始化
//arr：自动重装值。
//psc：时钟预分频数
//定时器溢出时间计算方法:Tout=((arr+1)*(psc+1))/Ft us.
//Ft=定时器工作频率,单位:Mhz
void TIM2_CH1_TIM4_CH1_Cap_Init(u32 arr,u16 psc)
{
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	TIM_ICInitTypeDef TIM2_ICInitStructure,
										TIM4_ICInitStructure;
	//TIM2配置----------------------------------------------------------------
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2,ENABLE);          //TIM2时钟使能    

	GPIO_PinAFConfig(GPIOA,GPIO_PinSource15,GPIO_AF_TIM2); //PA15复用位定时器2

	TIM_TimeBaseStructure.TIM_Prescaler=psc;  //定时器分频
	TIM_TimeBaseStructure.TIM_CounterMode=TIM_CounterMode_Up; //向上计数模式
	TIM_TimeBaseStructure.TIM_Period=arr;   //自动重装载值
	TIM_TimeBaseStructure.TIM_ClockDivision=TIM_CKD_DIV1; 

	TIM_TimeBaseInit(TIM2,&TIM_TimeBaseStructure);

	//初始化TIM2输入捕获参数
	TIM2_ICInitStructure.TIM_Channel = TIM_Channel_1; //CC1S=01         选择输入端 IC1映射到TI1上
	TIM2_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;        //上升沿捕获
	TIM2_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI; //映射到TI1上
	TIM2_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;         //配置输入分频,不分频 
	TIM2_ICInitStructure.TIM_ICFilter = 0x0f;//IC1F=0000 配置输入滤波器 不滤波
	TIM_ICInit(TIM2, &TIM2_ICInitStructure);

	TIM_ITConfig(TIM2,TIM_IT_Update|TIM_IT_CC1,ENABLE);//允许更新中断 ,允许CC1IE捕获中断        

	TIM_Cmd(TIM2,ENABLE );         //使能定时器2

	NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=1;//抢占优先级
	NVIC_InitStructure.NVIC_IRQChannelSubPriority =1;                //子优先级
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;                        //IRQ通道使能
	NVIC_Init(&NVIC_InitStructure);        //根据指定的参数初始化VIC寄存器   
	
	//TIM4配置----------------------------------------------------------------
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4,ENABLE);          //TIM4时钟使能    

	GPIO_PinAFConfig(GPIOD,GPIO_PinSource12,GPIO_AF_TIM4); //PD12复用位定时器4

	TIM_TimeBaseStructure.TIM_Prescaler=psc;  //定时器分频
	TIM_TimeBaseStructure.TIM_CounterMode=TIM_CounterMode_Up; //向上计数模式
	TIM_TimeBaseStructure.TIM_Period=arr;   //自动重装载值
	TIM_TimeBaseStructure.TIM_ClockDivision=TIM_CKD_DIV1; 

	TIM_TimeBaseInit(TIM4,&TIM_TimeBaseStructure);

	//初始化TIM4输入捕获参数
	TIM4_ICInitStructure.TIM_Channel = TIM_Channel_1; //CC1S=01         选择输入端 IC1映射到TI1上
	TIM4_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;        //上升沿捕获
	TIM4_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI; //映射到TI1上
	TIM4_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;         //配置输入分频,不分频 
	TIM4_ICInitStructure.TIM_ICFilter = 0x0f;//IC1F=0000 配置输入滤波器 不滤波
	TIM_ICInit(TIM4, &TIM4_ICInitStructure);

	TIM_ITConfig(TIM4,TIM_IT_CC1,ENABLE);//允许更新中断 ,允许CC1IE捕获中断        

	TIM_Cmd(TIM4,ENABLE );         //使能定时器4

	NVIC_InitStructure.NVIC_IRQChannel = TIM4_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=1;//抢占优先级
	NVIC_InitStructure.NVIC_IRQChannelSubPriority =1;                //子优先级
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;                        //IRQ通道使能
	NVIC_Init(&NVIC_InitStructure);        //根据指定的参数初始化VIC寄存器   
}

//---------------------------以上为测相位初始化-----------------------------------//

//初始化，切换模式使用
void ALL_UsedTIM_DEInit()
{
	RCC_APB1PeriphResetCmd(RCC_APB1Periph_TIM2, ENABLE);
	RCC_APB1PeriphResetCmd(RCC_APB1Periph_TIM2, DISABLE);
	RCC_APB1PeriphResetCmd(RCC_APB1Periph_TIM3, ENABLE);
	RCC_APB1PeriphResetCmd(RCC_APB1Periph_TIM3, DISABLE);
	RCC_APB1PeriphResetCmd(RCC_APB1Periph_TIM4, ENABLE);
	RCC_APB1PeriphResetCmd(RCC_APB1Periph_TIM4, DISABLE);
	RCC_APB1PeriphResetCmd(RCC_APB1Periph_TIM5, ENABLE);
	RCC_APB1PeriphResetCmd(RCC_APB1Periph_TIM5, DISABLE);
	rising_flag = 0;
	CLK_NUM = 0;
	FINISH = 0;
}


//---------------------------以下为测相位初始化-----------------------------------//

//通用定时器TIM2_CH1_TIM5_CH1输入捕获初始化
//arr：自动重装值。
//psc：时钟预分频数
//定时器溢出时间计算方法:Tout=((arr+1)*(psc+1))/Ft us.
//Ft=定时器工作频率,单位:Mhz
void TIM2_CH1_TIM5_CH1_Cap_Init(u32 arr,u16 psc)
{
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	TIM_ICInitTypeDef TIM2_ICInitStructure,
										TIM5_ICInitStructure;
	//TIM2配置----------------------------------------------------------------
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2,ENABLE);          //TIM2时钟使能    

	GPIO_PinAFConfig(GPIOA,GPIO_PinSource15,GPIO_AF_TIM2); //PA15复用位定时器2

	TIM_TimeBaseStructure.TIM_Prescaler=psc;  //定时器分频
	TIM_TimeBaseStructure.TIM_CounterMode=TIM_CounterMode_Up; //向上计数模式
	TIM_TimeBaseStructure.TIM_Period=arr;   //自动重装载值
	TIM_TimeBaseStructure.TIM_ClockDivision=TIM_CKD_DIV1; 

	TIM_TimeBaseInit(TIM2,&TIM_TimeBaseStructure);

	//初始化TIM2输入捕获参数
	TIM2_ICInitStructure.TIM_Channel = TIM_Channel_1; //CC1S=01         选择输入端 IC1映射到TI1上
	TIM2_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;        //上升沿捕获
	TIM2_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI; //映射到TI1上
	TIM2_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;         //配置输入分频,不分频 
	TIM2_ICInitStructure.TIM_ICFilter = 0x0f;//IC1F=0000 配置输入滤波器
	TIM_ICInit(TIM2, &TIM2_ICInitStructure);

	TIM_ITConfig(TIM2,TIM_IT_Update|TIM_IT_CC1,ENABLE);//允许更新中断 ,允许CC1IE捕获中断        

	TIM_Cmd(TIM2,ENABLE );         //使能定时器2

	NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=1;//抢占优先级
	NVIC_InitStructure.NVIC_IRQChannelSubPriority =1;                //子优先级
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;                        //IRQ通道使能
	NVIC_Init(&NVIC_InitStructure);        //根据指定的参数初始化VIC寄存器   
	
	//TIM5配置----------------------------------------------------------------
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5,ENABLE);          //TIM5时钟使能    

	GPIO_PinAFConfig(GPIOA,GPIO_PinSource0,GPIO_AF_TIM5); //PA0复用位定时器5

	TIM_TimeBaseStructure.TIM_Prescaler=psc;  //定时器分频
	TIM_TimeBaseStructure.TIM_CounterMode=TIM_CounterMode_Up; //向上计数模式
	TIM_TimeBaseStructure.TIM_Period=arr;   //自动重装载值
	TIM_TimeBaseStructure.TIM_ClockDivision=TIM_CKD_DIV1; 

	TIM_TimeBaseInit(TIM5,&TIM_TimeBaseStructure);

	//初始化TIM4输入捕获参数
	TIM5_ICInitStructure.TIM_Channel = TIM_Channel_1; //CC1S=01         选择输入端 IC1映射到TI1上
	TIM5_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;        //上升沿捕获
	TIM5_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI; //映射到TI1上
	TIM5_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;         //配置输入分频,不分频 
	TIM5_ICInitStructure.TIM_ICFilter = 0x0f;//IC1F=0000 配置输入滤波器
	TIM_ICInit(TIM5, &TIM5_ICInitStructure);

	TIM_ITConfig(TIM5,TIM_IT_CC1,ENABLE);//允许更新中断 ,允许CC1IE捕获中断        

	TIM_Cmd(TIM5,ENABLE );         //使能定时器4

	NVIC_InitStructure.NVIC_IRQChannel = TIM5_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=1;//抢占优先级
	NVIC_InitStructure.NVIC_IRQChannelSubPriority =1;                //子优先级
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;                        //IRQ通道使能
	NVIC_Init(&NVIC_InitStructure);        //根据指定的参数初始化VIC寄存器   
}

//通用定时器TIM2_CH1_CH2输入捕获初始化
//arr：自动重装值。
//psc：时钟预分频数
//定时器溢出时间计算方法:Tout=((arr+1)*(psc+1))/Ft us.
//Ft=定时器工作频率,单位:Mhz
void TIM2_CH1_CH2_Cap_Init(u32 arr,u16 psc)
{
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	NVIC_InitTypeDef NVIC_InitStructure;
	TIM_ICInitTypeDef TIM2_ICInitStructure;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2,ENABLE);          //TIM2时钟使能    

	GPIO_PinAFConfig(GPIOA,GPIO_PinSource15,GPIO_AF_TIM2); //PA15复用位定时器2
	GPIO_PinAFConfig(GPIOA,GPIO_PinSource1,GPIO_AF_TIM2); //PA1复用位定时器2

	TIM_TimeBaseStructure.TIM_Prescaler=psc;  //定时器分频
	TIM_TimeBaseStructure.TIM_CounterMode=TIM_CounterMode_Up; //向上计数模式
	TIM_TimeBaseStructure.TIM_Period=arr;   //自动重装载值
	TIM_TimeBaseStructure.TIM_ClockDivision=TIM_CKD_DIV1; 

	TIM_TimeBaseInit(TIM2,&TIM_TimeBaseStructure);

	//初始化TIM2输入捕获参数
	TIM2_ICInitStructure.TIM_Channel = TIM_Channel_1; //CC1S=01         选择输入端 IC1映射到TI1上
	TIM2_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;        //上升沿捕获
	TIM2_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI; //映射到TI1上
	TIM2_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;         //配置输入分频,不分频 
	TIM2_ICInitStructure.TIM_ICFilter = 0x0f;//IC1F=0000 配置输入滤波器 不滤波
	TIM_ICInit(TIM2, &TIM2_ICInitStructure);
	
	TIM2_ICInitStructure.TIM_Channel = TIM_Channel_2; //CC1S=01         选择输入端 IC1映射到TI1上
	TIM2_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;        //上升沿捕获
	TIM2_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI; //映射到TI1上
	TIM2_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;         //配置输入分频,不分频 
	TIM2_ICInitStructure.TIM_ICFilter = 0x0f;//IC1F=0000 配置输入滤波器 不滤波
	TIM_ICInit(TIM2, &TIM2_ICInitStructure);

	TIM_ITConfig(TIM2,TIM_IT_Update|TIM_IT_CC1,ENABLE);//允许更新中断 ,允许CC1IE捕获中断       
	TIM_Cmd(TIM2,ENABLE );         //使能定时器2

	NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority=1;//抢占优先级
	NVIC_InitStructure.NVIC_IRQChannelSubPriority =1;                //子优先级
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;                        //IRQ通道使能
	NVIC_Init(&NVIC_InitStructure);        //根据指定的参数初始化VIC寄存器   
}



