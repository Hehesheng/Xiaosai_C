#include "sys.h"

#define Cal_Low 0//低频模式
#define Cal_Hig 1//高频模式
#define Cal_Zkb 2//占空比模式
#define Cal_Cxw 3//测量相位模式

extern u32 CLK_NUM;//计数
extern u32 rising_first,rising_second,falling;//两个上升沿 的计数器数据存储以及 下降沿 数据存储
extern u8 FINISH;//完成标志
extern u8 selet_time;//标志模式是否切换
extern u8 rising_flag;//判断是否第一次上升
extern u8 mode_flag;//模式选择（低频模式，高频模式，测量占空比模式）

void TIM3_Int_Init(u16 arr,u16 psc);//TIM3中断控制
void TIM2_Counter_Init(void);//使用外部时钟计时
void TIM2_CH1_Cap_Init(u32 arr,u16 psc);//TIM2_CH1的输入捕获初始化
void TIM5_CH1_CH2_Cap_Init(u32 arr,u16 psc);//TIM5_CH!_CH2的测相位初始化
void ALL_UsedTIM_DEInit(void);//初始化，切换模式使用
