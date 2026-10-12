#include "OLED_UI.h"
#include "OLED_UI_MenuData.h"
/*目前例程为测试版，代码仍在优化当中*/
/*接线方式：
scl pb8 
sda pb9
res pb5
dc  pb6
cs  pb7
确认按键 pb12
取消按键 pb13
上按键   pb14
下按键   pb15
 编码器a pa8
 编码器b pa9
 编码器按键 pb12
*/

int main(){
	
	OLED_UI_Init(&MainMenuPage);

	while(1){

		OLED_UI_MainLoop();

	}
}

//中断函数
void TIM4_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM4, TIM_IT_Update) == SET)
	{
		OLED_UI_InterruptHandler();
		
		TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
	}
}
