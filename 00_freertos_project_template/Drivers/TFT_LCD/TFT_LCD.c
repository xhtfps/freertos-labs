/**
 * @file Drivers/TFT_LCD/TFT_LCD.c
 * @brief TFT LCD 控制器驱动、基础绘图和显示状态对象模块。
 * @details
 * - 属于显示子系统，分别负责 LCD、字库 Flash、字体数据或文字渲染中的明确职责。
 * - 模块间通过对象接口或兼容 API 协作，避免 UI 层直接操作显示控制器寄存器。
 * - 字体/字模数据文件以数组尺寸、字号和索引关系一致性为维护重点。
 * - 本文件已移除原子/ST 示例模板，统一使用项目自定义 UTF-8 文件模板。
 * @encoding UTF-8
 */

#include "TFT_LCD.h"
#include "fonts.h"
#if _USE_HZK16==1
#elif _USE_HZK16==2
#define HZK_Address ((uint32_t)0X80BE804)
#endif

/* LCD 使用 FSMC 16 位并口映射：寄存器地址和数据地址连续映射。 */
typedef struct
{
	__IO uint16_t LCD_REG;
	__IO uint16_t LCD_RAM;
} LCD_TypeDef;

#define LCD_BASE           ((uint32_t)(0x60000000))
#define LCD                ((LCD_TypeDef *) LCD_BASE)

static sFONT *LCD_Currentfonts=&LCD_DEFAULT_FONT;

__IO uint16_t TextColor = 0x0000, BackColor = 0xFFFF;
SRAM_HandleTypeDef g_lcd_sram_handler;

static void TftLcdDevice_Construct(TftLcdDevice *self);
static void TftLcdDevice_Init(TftLcdDevice *self);
static void TftLcdDevice_SetColors(TftLcdDevice *self, uint16_t text_color, uint16_t back_color);
static void TftLcdDevice_GetColors(TftLcdDevice *self, __IO uint16_t *text_color, __IO uint16_t *back_color);
static void TftLcdDevice_SetTextColor(TftLcdDevice *self, uint16_t color);
static void TftLcdDevice_SetBackColor(TftLcdDevice *self, uint16_t color);
static void TftLcdDevice_SetFont(TftLcdDevice *self, sFONT *fonts, uint8_t mode);
static sFONT *TftLcdDevice_GetFont(TftLcdDevice *self);
static void TftLcdDevice_Clear(TftLcdDevice *self, uint16_t color);
static void TftLcdDevice_DrawPoint(TftLcdDevice *self, uint16_t x, uint16_t y, uint16_t color);
static void TftLcdDevice_WriteReg(TftLcdDevice *self, uint16_t reg, uint16_t value);
static uint16_t TftLcdDevice_ReadReg(TftLcdDevice *self, uint16_t reg);

static const TftLcdDeviceOps s_tft_lcd_ops =
{
	TftLcdDevice_Init,
	TftLcdDevice_SetColors,
	TftLcdDevice_GetColors,
	TftLcdDevice_SetTextColor,
	TftLcdDevice_SetBackColor,
	TftLcdDevice_SetFont,
	TftLcdDevice_GetFont,
	TftLcdDevice_Clear,
	TftLcdDevice_DrawPoint,
	TftLcdDevice_WriteReg,
	TftLcdDevice_ReadReg,
};

TftLcdDevice g_tft_lcd;

/* LCD 底层写寄存器/写显存接口。 */
__inline void LCD_WriteRAM_Prepare(void);
__inline void LCD_REG_Select(uint16_t REG_Code);
__inline void LCD_WriteRAM(uint16_t RGB_Code);

static void TftLcdDevice_Construct(TftLcdDevice *self)
{
	if (self->ops == NULL)
	{
		self->sram_handle = &g_lcd_sram_handler;
		self->font = LCD_Currentfonts;
		self->text_color = TextColor;
		self->back_color = BackColor;
		self->ops = &s_tft_lcd_ops;
	}
}







static void TftLcdDevice_Init(TftLcdDevice *self)
{
	(void)self;
	/* 先配置 GPIO/FSMC，再执行屏幕控制器初始化序列。 */
	LCD_CtrlLinesConfig();
	LCD_FSMCConfig();
	HAL_Delay(50); 
	/* 下方初始化序列由 LCD 控制器型号决定。
	 * 这些寄存器值本质上是屏幕时序/电源/Gamma 参数表，不在业务层拆散维护。
	 */
#if __TFT_ID==0xB505
	
	LCD_WriteReg(LCD_REG_229,0x8000); 
	LCD_WriteReg(LCD_REG_0,  0x0001); 
	LCD_WriteReg(LCD_REG_1,  0x0000);   
	LCD_WriteReg(LCD_REG_2,  0x0700); 
	LCD_WriteReg(LCD_REG_3,  0x1018); 
	LCD_WriteReg(LCD_REG_4,  0x0000); 
	LCD_WriteReg(LCD_REG_8,  0x0202); 
	LCD_WriteReg(LCD_REG_9,  0x0000); 
	LCD_WriteReg(LCD_REG_10, 0x0000); 
	LCD_WriteReg(LCD_REG_12, 0x0000); 
	LCD_WriteReg(LCD_REG_13, 0x0000); 
	LCD_WriteReg(LCD_REG_15, 0x0000); 
	
	LCD_WriteReg(LCD_REG_16, 0x0000); 
	LCD_WriteReg(LCD_REG_17, 0x0000); 
	LCD_WriteReg(LCD_REG_18, 0x0000); 
	LCD_WriteReg(LCD_REG_19, 0x0000); 
	HAL_Delay(100);                 
	LCD_WriteReg(LCD_REG_16, 0x17B0); 
	LCD_WriteReg(LCD_REG_17, 0x0137); 
	HAL_Delay(10);                  
	LCD_WriteReg(LCD_REG_18, 0x0139); 
	HAL_Delay(10);                  
	LCD_WriteReg(LCD_REG_19, 0x1d00); 
	LCD_WriteReg(LCD_REG_41, 0x0013); 
	HAL_Delay(10);                  
	LCD_WriteReg(LCD_REG_32, 0x0000); 
	LCD_WriteReg(LCD_REG_33, 0x0000); 
	
	LCD_WriteReg(LCD_REG_48, 0x0006);
	LCD_WriteReg(LCD_REG_49, 0x0101);
	LCD_WriteReg(LCD_REG_50, 0x0003);
	LCD_WriteReg(LCD_REG_53, 0x0106);
	LCD_WriteReg(LCD_REG_54, 0x0b02);
	LCD_WriteReg(LCD_REG_55, 0x0302);
	LCD_WriteReg(LCD_REG_56, 0x0707);
	LCD_WriteReg(LCD_REG_57, 0x0007);
	LCD_WriteReg(LCD_REG_60, 0x0600);
	LCD_WriteReg(LCD_REG_61, 0x020b);
	
	LCD_WriteReg(LCD_REG_80, 0x0000); 
	LCD_WriteReg(LCD_REG_81, 0x00EF); 
	LCD_WriteReg(LCD_REG_82, 0x0000); 
	LCD_WriteReg(LCD_REG_83, 0x013F); 
	LCD_WriteReg(LCD_REG_96,  0x2700); 
	LCD_WriteReg(LCD_REG_97,  0x0001); 
	LCD_WriteReg(LCD_REG_106, 0x0000); 
	
	LCD_WriteReg(LCD_REG_128, 0x0000);
	LCD_WriteReg(LCD_REG_129, 0x0000);
	LCD_WriteReg(LCD_REG_130, 0x0000);
	LCD_WriteReg(LCD_REG_131, 0x0000);
	LCD_WriteReg(LCD_REG_132, 0x0000);
	LCD_WriteReg(LCD_REG_133, 0x0000);
	
	LCD_WriteReg(LCD_REG_144, 0x0010);
	LCD_WriteReg(LCD_REG_146, 0x0000);
	LCD_WriteReg(LCD_REG_147, 0x0003);
	LCD_WriteReg(LCD_REG_149, 0x0110);
	LCD_WriteReg(LCD_REG_151, 0x0000);
	LCD_WriteReg(LCD_REG_152, 0x0000);
	
	
	
	LCD_WriteReg(LCD_REG_3, 0x1000);	    
	LCD_WriteReg(LCD_REG_7, 0x0173); 
#elif __TFT_ID==0x4531
	
	LCD_WriteReg(0X00,0X0001);
	HAL_Delay(10);
	LCD_WriteReg(0X10,0X1628);
	LCD_WriteReg(0X12,0X000e);
	LCD_WriteReg(0X13,0X0A39);
	HAL_Delay(10);
	LCD_WriteReg(0X11,0X0040);
	LCD_WriteReg(0X15,0X0050);
	HAL_Delay(10);
	LCD_WriteReg(0X12,0X001e);
	HAL_Delay(10);
	LCD_WriteReg(0X10,0X1620);
	LCD_WriteReg(0X13,0X2A39);
	HAL_Delay(10);
	LCD_WriteReg(0X01,0X0100);
	LCD_WriteReg(0X02,0X0300);
	LCD_WriteReg(0X03,0X1030);
	LCD_WriteReg(0X08,0X0202);
	LCD_WriteReg(0X0A,0X0008);
	LCD_WriteReg(0X30,0X0000);
	LCD_WriteReg(0X31,0X0402);
	LCD_WriteReg(0X32,0X0106);
	LCD_WriteReg(0X33,0X0503);
	LCD_WriteReg(0X34,0X0104);
	LCD_WriteReg(0X35,0X0301);
	LCD_WriteReg(0X36,0X0707);
	LCD_WriteReg(0X37,0X0305);
	LCD_WriteReg(0X38,0X0208);
	LCD_WriteReg(0X39,0X0F0B);
	LCD_WriteReg(0X41,0X0002);
	LCD_WriteReg(0X60,0X2700);
	LCD_WriteReg(0X61,0X0001);
	LCD_WriteReg(0X90,0X0210);
	LCD_WriteReg(0X92,0X010A);
	LCD_WriteReg(0X93,0X0004);
	LCD_WriteReg(0XA0,0X0100);
	LCD_WriteReg(0X07,0X0001);
	LCD_WriteReg(0X07,0X0021);
	LCD_WriteReg(0X07,0X0023);
	LCD_WriteReg(0X07,0X0033);
	LCD_WriteReg(0X07,0X0133);
	LCD_WriteReg(0XA0,0X0000);
#elif __TFT_ID==0x9325
	LCD_WriteReg(0x00e7,0x0010);
	LCD_WriteReg(0x0000,0x0001);
	LCD_WriteReg(0x0001,0x0100);
	LCD_WriteReg(0x0002,0x0700);
	
	
	LCD_WriteReg(0x0003,(1<<12)|(3<<4)|(0<<3) );
	LCD_WriteReg(0x0004,0x0000);
	LCD_WriteReg(0x0008,0x0207);
	LCD_WriteReg(0x0009,0x0000);
	LCD_WriteReg(0x000a,0x0000);
	LCD_WriteReg(0x000c,0x0001);
	LCD_WriteReg(0x000d,0x0000);
	LCD_WriteReg(0x000f,0x0000);
	
	LCD_WriteReg(0x0010,0x0000);
	LCD_WriteReg(0x0011,0x0007);
	LCD_WriteReg(0x0012,0x0000);
	LCD_WriteReg(0x0013,0x0000);
	HAL_Delay(50);
	LCD_WriteReg(0x0010,0x1590);
	LCD_WriteReg(0x0011,0x0227);
	HAL_Delay(50);
	LCD_WriteReg(0x0012,0x009c);
	HAL_Delay(50);
	LCD_WriteReg(0x0013,0x1900);
	LCD_WriteReg(0x0029,0x0023);
	LCD_WriteReg(0x002b,0x000e);
	HAL_Delay(50);
	LCD_WriteReg(0x0020,0x0000);
	LCD_WriteReg(0x0021,0x013f);
	HAL_Delay(50);
	
	LCD_WriteReg(0x0030,0x0007);
	LCD_WriteReg(0x0031,0x0707);
	LCD_WriteReg(0x0032,0x0006);
	LCD_WriteReg(0x0035,0x0704);
	LCD_WriteReg(0x0036,0x1f04);
	LCD_WriteReg(0x0037,0x0004);
	LCD_WriteReg(0x0038,0x0000);
	LCD_WriteReg(0x0039,0x0706);
	LCD_WriteReg(0x003c,0x0701);
	LCD_WriteReg(0x003d,0x000f);
	HAL_Delay(50);
	LCD_WriteReg(0x0050,0x0000); 
	LCD_WriteReg(0x0051,0x00ef); 
	LCD_WriteReg(0x0052,0x0000); 
	LCD_WriteReg(0x0053,0x013f); 
	LCD_WriteReg(0x0060,0xa700);
	LCD_WriteReg(0x0061,0x0001);
	LCD_WriteReg(0x006a,0x0000);
	LCD_WriteReg(0x0080,0x0000);
	LCD_WriteReg(0x0081,0x0000);
	LCD_WriteReg(0x0082,0x0000);
	LCD_WriteReg(0x0083,0x0000);
	LCD_WriteReg(0x0084,0x0000);
	LCD_WriteReg(0x0085,0x0000);
	LCD_WriteReg(0x0090,0x0010);
	LCD_WriteReg(0x0092,0x0000);
	LCD_WriteReg(0x0093,0x0003);
	LCD_WriteReg(0x0095,0x0110);
	LCD_WriteReg(0x0097,0x0000);
	LCD_WriteReg(0x0098,0x0000);

	LCD_WriteReg(0x0007,0x0133);
	LCD_WriteReg(0x0020,0x0000);
	LCD_WriteReg(0x0021,0x013f);
#elif __TFT_ID==0x1936
	LCD_REG_Select(0x00E2);	  
	LCD_WriteRAM(0x0023);	    
	LCD_WriteRAM(0x0002);
	LCD_WriteRAM(0x0004);
	LCD_REG_Select(0x00E0);  
	LCD_WriteRAM(0x0001);
	HAL_Delay(1);
	LCD_REG_Select(0x00E0);
	LCD_WriteRAM(0x0003);
	HAL_Delay(5);
	LCD_REG_Select(0x0001);  
	HAL_Delay(5);
	LCD_REG_Select(0x00E6);	
	LCD_WriteRAM(0x0003);
	LCD_WriteRAM(0x00ff);
	LCD_WriteRAM(0x00ff);
	
	LCD_REG_Select(0x00B0);	
	LCD_WriteRAM(0x0027);
	LCD_WriteRAM(0x0000);
	LCD_WriteRAM((SSD1963_HDP>>8)&0X00FF);  
	LCD_WriteRAM(SSD1963_HDP&0X00FF);
	LCD_WriteRAM((SSD1963_VDP>>8)&0X00FF);  
	LCD_WriteRAM(SSD1963_VDP&0X00FF);
	LCD_WriteRAM(0x0000);
	LCD_REG_Select(0x00B4);	
	LCD_WriteRAM((SSD1963_HT>>8)&0X00FF);  
	LCD_WriteRAM(SSD1963_HT&0X00FF);
	LCD_WriteRAM((SSD1963_HPS>>8)&0X00FF);  
	LCD_WriteRAM(SSD1963_HPS&0X00FF);
	LCD_WriteRAM(SSD1963_HPW);			   
	LCD_WriteRAM((SSD1963_LPS>>8)&0X00FF);  
	LCD_WriteRAM(SSD1963_LPS&0X00FF);
	LCD_WriteRAM(0x0000);
	LCD_REG_Select(0x00B6);	
	LCD_WriteRAM((SSD1963_VT>>8)&0X00FF);   
	LCD_WriteRAM(SSD1963_VT&0X00FF);
	LCD_WriteRAM((SSD1963_VPS>>8)&0X00FF);  
	LCD_WriteRAM(SSD1963_VPS&0X00FF);
	LCD_WriteRAM(SSD1963_VPW);			   
	LCD_WriteRAM((SSD1963_FPS>>8)&0X00FF);  
	LCD_WriteRAM(SSD1963_FPS&0X00FF);
	LCD_REG_Select(0x00BA);
	LCD_WriteRAM(0x000F);    
	LCD_REG_Select(0x00B8);
	LCD_WriteRAM(0x0007);    
	LCD_WriteRAM(0x0001);    
	LCD_REG_Select(0x0036); 
	LCD_WriteRAM(0x0000);
	LCD_REG_Select(0x00F0); 
	LCD_WriteRAM(0x0003);
	HAL_Delay(5);
	
	LCD_REG_Select(0xBC);
	LCD_WriteRAM(0x40);
	LCD_WriteRAM(0x80);
	LCD_WriteRAM(0x40);
	LCD_WriteRAM(0x01);
	HAL_Delay(10);
	LCD_REG_Select(0x0029); 
	
	LCD_REG_Select(0xBE); 
	LCD_WriteRAM(0x0E); 
	LCD_WriteRAM(0xFF); 
	LCD_WriteRAM(0x09); 
	LCD_WriteRAM(0xFF); 
	LCD_WriteRAM(0x00); 
	LCD_WriteRAM(0x00); 
	LCD_REG_Select(0xD4);
	
	
	LCD_WriteRAM(0x00); 
	LCD_WriteRAM(0x1C); 
	LCD_WriteRAM(0x20); 


	LCD_WriteRAM(0x00); 
	LCD_WriteRAM(0x46); 
	LCD_WriteRAM(0x50); 


	LCD_WriteRAM(0x00); 
	LCD_WriteRAM(0xA8); 
	LCD_WriteRAM(0xC0); 
	LCD_REG_Select(0x00d0);  
	LCD_WriteRAM(0x000d);
	LCD_SetDisplayWindow(0,0,799,479);
#endif
	self->sram_handle = &g_lcd_sram_handler;
	self->font = LCD_Currentfonts;
	self->text_color = TextColor;
	self->back_color = BackColor;
}

void TFT_LCD_Init(void)
{
	TftLcdDevice_Construct(&g_tft_lcd);
	g_tft_lcd.ops->init(&g_tft_lcd);
}







static void TftLcdDevice_SetColors(TftLcdDevice *self, uint16_t text_color, uint16_t back_color)
{
	TextColor = text_color;
	BackColor = back_color;
	self->text_color = text_color;
	self->back_color = back_color;
}

void LCD_SetColors(__IO uint16_t _TextColor, __IO uint16_t _BackColor)
{
	TftLcdDevice_Construct(&g_tft_lcd);
	g_tft_lcd.ops->set_colors(&g_tft_lcd, _TextColor, _BackColor);
}






static void TftLcdDevice_GetColors(TftLcdDevice *self, __IO uint16_t *text_color, __IO uint16_t *back_color)
{
	self->text_color = TextColor;
	self->back_color = BackColor;
	*text_color = self->text_color;
	*back_color = self->back_color;
}

void LCD_GetColors(__IO uint16_t *_TextColor, __IO uint16_t *_BackColor)
{
	TftLcdDevice_Construct(&g_tft_lcd);
	g_tft_lcd.ops->get_colors(&g_tft_lcd, _TextColor, _BackColor);
}






static void TftLcdDevice_SetTextColor(TftLcdDevice *self, uint16_t color)
{
	TextColor = color;
	self->text_color = color;
}

void LCD_SetTextColor(__IO uint16_t Color)
{
	TftLcdDevice_Construct(&g_tft_lcd);
	g_tft_lcd.ops->set_text_color(&g_tft_lcd, Color);
}






static void TftLcdDevice_SetBackColor(TftLcdDevice *self, uint16_t color)
{
	BackColor = color;
	self->back_color = color;
}

void LCD_SetBackColor(__IO uint16_t Color)
{
	TftLcdDevice_Construct(&g_tft_lcd);
	g_tft_lcd.ops->set_back_color(&g_tft_lcd, Color);
}






static void TftLcdDevice_Clear(TftLcdDevice *self, uint16_t color)
{
	uint32_t index = 0;
	(void)self;
	LCD_SetCursor(0x00, 0x00);
	LCD_WriteRAM_Prepare(); 
	for(index = 0; index < LCD_Pixel_Max; index++)
		LCD->LCD_RAM = color;
}

void LCD_Clear(uint16_t Color)
{
	TftLcdDevice_Construct(&g_tft_lcd);
	g_tft_lcd.ops->clear(&g_tft_lcd, Color);
}








void LCD_Appoint_Clear(uint16_t Xs,uint16_t Ys,uint16_t Xe,uint16_t Ye,uint16_t Color)
{
	uint32_t i,j;
	for(i=0; i<Xe-Xs; i++)
	{
		for (j = 0; j <Ye-Ys; j++)
		{
			LCD_SetCursor(Xs+i,Ys+j);
			LCD_WriteRAM_Prepare();
			LCD->LCD_RAM =Color;
		}
	}
}





static void TftLcdDevice_SetFont(TftLcdDevice *self, sFONT *fonts, uint8_t mode)
{
	LCD_Currentfonts = fonts;
	LCD_Currentfonts->Mode=mode;
	self->font = LCD_Currentfonts;
}

void LCD_SetFont(sFONT *fonts,uint8_t Mode)
{
	TftLcdDevice_Construct(&g_tft_lcd);
	g_tft_lcd.ops->set_font(&g_tft_lcd, fonts, Mode);
}






static sFONT *TftLcdDevice_GetFont(TftLcdDevice *self)
{
	self->font = LCD_Currentfonts;
	return self->font;
}

sFONT *LCD_GetFont(void)
{
	TftLcdDevice_Construct(&g_tft_lcd);
	return g_tft_lcd.ops->get_font(&g_tft_lcd);
}







void LCD_ShowChar(uint16_t x,uint16_t y,uint8_t Char)
{
	uint16_t temp,dy,dx,K;
	if(Char>31)
		Char=Char-32;
	else  
		Char=0;
	for(dy=0,K=0; dy<LCD_Currentfonts->Height; dy++)
	{
		temp=LCD_Currentfonts->table[Char][K++];
#if   __FONTS==1
		for(dx=0; dx<LCD_Currentfonts->Height/2; dx++)
		{
			if(temp&0x01)
			{
				LCD_SetCursor(x+dx,y+dy);
				LCD_WriteRAM_Prepare();
				LCD_WriteRAM(TextColor);
			}
			else if(LCD_Currentfonts->Mode)
			{
				LCD_SetCursor(x+dx,y+dy);
				LCD_WriteRAM_Prepare();
				LCD_WriteRAM(BackColor);
			}
			temp>>=1;
		}
#elif __FONTS==2
		for(dx=0; dx<8; dx++)
		{
			if(temp&0x01)
			{
				LCD_SetCursor(x+dx,y+dy);
				LCD_WriteRAM_Prepare();
				LCD_WriteRAM(TextColor);
			}
			else if(LCD_Currentfonts->Mode)
			{
				LCD_SetCursor(x+dx,y+dy);
				LCD_WriteRAM_Prepare();
				LCD_WriteRAM(BackColor);
			}
			temp>>=1;
		}
		temp=LCD_Currentfonts->table[Char][K++];
		for(; dx<LCD_Currentfonts->Width; dx++)
		{
			if(temp&0x01)
			{
				LCD_SetCursor(x+dx,y+dy);
				LCD_WriteRAM_Prepare();
				LCD_WriteRAM(TextColor);
			}
			else if(LCD_Currentfonts->Mode)
			{
				LCD_SetCursor(x+dx,y+dy);
				LCD_WriteRAM_Prepare();
				LCD_WriteRAM(BackColor);
			}
			temp>>=1;
		}
#endif
	}
}







#if _USE_HZK16

#if   __FONTS==1
static void TFT_DispChar(uint16_t _Xpos, uint16_t _Ypos, const uint8_t *c, uint8_t _width)
{
	uint32_t height = 0, i = 0;
	uint16_t X_add = _Xpos;
	uint16_t Y_add = _Ypos;
	LCD_SetCursor(_Xpos, _Ypos);
	if (LCD_Currentfonts->Mode)
	{
		for (height = 0; height < 16; height++)  
		{
			X_add = _Xpos;
			for (i = 0; i < _width; i++)	  
			{
				LCD_SetCursor(X_add, Y_add);
				LCD_WriteRAM_Prepare();
				if ((c[height * (_width / 8) + i / 8] & (0x80 >> (i % 8))) == 0x00)
					LCD_WriteRAM(BackColor);
				else
					LCD_WriteRAM(TextColor);
				X_add++;
			}
			Y_add++;
		}
	}
	else
	{
		for (height = 0; height < 16; height++)   
		{
			X_add = _Xpos;
			for (i = 0; i < _width; i++)	   
			{
				if ((c[height * (_width / 8) + i / 8] & (0x80 >> (i % 8))) != 0x00)
				{
					LCD_SetCursor(X_add, Y_add);
					LCD_WriteRAM_Prepare();
					LCD->LCD_RAM = TextColor;
				}
				X_add++;
			}
			Y_add++;
		}
	}
}
#elif __FONTS==2

static void TFT_DispChar(uint16_t _Xpos, uint16_t _Ypos, const uint8_t *c, uint8_t _width)
{
	uint32_t height = 0, i = 0;
	uint16_t X_add = _Xpos;
	uint16_t Y_add = _Ypos;
	LCD_SetCursor(_Xpos, _Ypos);
	if (LCD_Currentfonts->Mode)
	{
		for (height = 0; height < 16; height++)  
		{
			X_add = _Xpos;
			for (i = 0; i < _width; i++)	  
			{
				if ((c[height * (_width / 8) + i / 8] & (0x80 >> (i % 8))) == 0x00)
				{
					LCD_SetCursor(X_add, Y_add);
					LCD_WriteRAM_Prepare();
					LCD->LCD_RAM = BackColor;
					LCD_SetCursor(X_add+1, Y_add);
					LCD_WriteRAM_Prepare();
					LCD->LCD_RAM = BackColor;
					LCD_SetCursor(X_add, Y_add+1);
					LCD_WriteRAM_Prepare();
					LCD->LCD_RAM = BackColor;
					LCD_SetCursor(X_add+1, Y_add+1);
					LCD_WriteRAM_Prepare();
					LCD->LCD_RAM = BackColor;
				}
				else
				{
					LCD_SetCursor(X_add, Y_add);
					LCD_WriteRAM_Prepare();
					LCD->LCD_RAM = TextColor;
					LCD_SetCursor(X_add+1, Y_add);
					LCD_WriteRAM_Prepare();
					LCD->LCD_RAM = TextColor;
					LCD_SetCursor(X_add, Y_add+1);
					LCD_WriteRAM_Prepare();
					LCD->LCD_RAM = TextColor;
					LCD_SetCursor(X_add+1, Y_add+1);
					LCD_WriteRAM_Prepare();
					LCD->LCD_RAM = TextColor;
				}
				X_add+=2;
			}
			Y_add+=2;
		}
	}
	else
	{
		for (height = 0; height < 16; height++)   
		{
			X_add = _Xpos;
			for (i = 0; i < _width; i++)	   
			{
				if ((c[height * (_width / 8) + i / 8] & (0x80 >> (i % 8))) != 0x00)
				{
					LCD_SetCursor(X_add, Y_add);
					LCD_WriteRAM_Prepare();
					LCD->LCD_RAM = TextColor;
					LCD_SetCursor(X_add+1, Y_add);
					LCD_WriteRAM_Prepare();
					LCD->LCD_RAM = TextColor;
					LCD_SetCursor(X_add, Y_add+1);
					LCD_WriteRAM_Prepare();
					LCD->LCD_RAM = TextColor;
					LCD_SetCursor(X_add+1, Y_add+1);
					LCD_WriteRAM_Prepare();
					LCD->LCD_RAM = TextColor;
				}
				X_add+=2;
			}
			Y_add+=2;
		}
	}
}

#endif

#endif







uint32_t LCD_DisplayStringLine(uint16_t Line,uint16_t Column, uint8_t *ptr)
{
#if _USE_HZK16
	uint32_t address;
	uint16_t HZK16[16];
#endif
#if _USE_HZK16==2
	uint32_t *HZK=(uint32_t*)HZK16;
#endif
	uint16_t refcolumn =Column;
	uint16_t  refLine =Line;
#if _USE_HZK16
	uint8_t code1,code2;
#endif
	if(refLine>=LCD_Pixel_Height)
	{
		refLine=0;
		LCD_Currentfonts->Mode=1;
	}
	while (*ptr!='\0')
	{
#if _USE_HZK16
		code1=*ptr;
		if(code1<0x80)	
		{
#endif
			if(refcolumn>=LCD_Pixel_Width)
			{
				refcolumn=0;
				refLine+=LCD_Currentfonts->Height;
				if(refLine>=LCD_Pixel_Height)
				{
					refLine=0;
					LCD_Currentfonts->Mode=1;
				}
			}
			LCD_ShowChar(refcolumn, refLine, *ptr);
			refcolumn += LCD_Currentfonts->Width;
#if  _USE_HZK16
		}
		
		else
		{
			code2 = *++ptr;
			if(code2=='\0')
				return code1;
			
			address =((code1-0xa1) * 94 + (code2-0xa1))<<5;
#if _USE_HZK16==1
			SRAM_ReadBuffer(HZK16, address, 16);
#elif _USE_HZK16==2
			address+=HZK_Address;
			for(code1=0; code1<8; code1++)
			{
				HZK[code1]=(*(__IO uint32_t*)address);
				address+=4;
			}
#endif
			if(refcolumn>=LCD_Pixel_Width-LCD_Currentfonts->Height)
			{
				refcolumn=0;
				refLine+=LCD_Currentfonts->Height;
				if(refLine>=LCD_Pixel_Height)
				{
					refLine=0;
					LCD_Currentfonts->Mode=1;
				}
			}
			TFT_DispChar(refcolumn, refLine, (const uint8_t *)HZK16, 16);
			refcolumn +=LCD_Currentfonts->Width*2;
		}
#endif
		ptr++;
	}
	return 0;
}








#if _USE_0X

static void	Display_Control(uint8_t *refLine,uint16_t *refcolumn,uint8_t Char)
{
	if(*refcolumn>=LCD_Pixel_Width-LCD_Currentfonts->Width)
	{
		*refcolumn=0;
		*refLine+=LCD_Currentfonts->Height;
		if(*refLine>=LCD_Pixel_Height)
		{
			*refLine=0;
			LCD_Currentfonts->Mode=1;
		}
	}
	LCD_ShowChar(*refcolumn,*refLine,Char);
	*refcolumn+=LCD_Currentfonts->Width;
}







void LCD_Display0x(uint16_t Line,uint16_t Column, uint8_t *X,uint8_t N)
{
	uint16_t refcolumn =Column;
	uint8_t  i=0,temp=0, refLine =Line;
	for(i=0; i<N; i++)
	{
		temp=X[i]/16;
		if(temp<10)
			temp+='0';
		else
			temp+=55;
		Display_Control(&refLine,&refcolumn,temp);
		temp=X[i]%16;
		if(temp<10)
			temp+='0';
		else
			temp+=55;
		Display_Control(&refLine,&refcolumn,temp);
		Display_Control(&refLine,&refcolumn,' ');
	}
}
#endif








void LCD_DisplayNum(uint16_t Line,uint16_t Column, uint32_t num)
{
	uint32_t temp=num;
	uint16_t refcolumn =Column;
	uint16_t  refLine =Line,len=0;

	do
	{
		temp = temp/10;
		len++;
	}
	while(temp);
	len--;
	
	refcolumn +=len*LCD_Currentfonts->Width;
	
	do
	{
		temp=num%10;
		LCD_ShowChar(refcolumn,refLine,temp+'0');
		refcolumn-=LCD_Currentfonts->Width;
		num=num/10;
	}
	while(len--);
}






#if _USE_Float

void LCD_Display_FloatNum(uint16_t Line,uint16_t Column, double num)
{
	uint64_t Temp;
	uint32_t Temp1;
	uint8_t len=0;
	if(num<0)
	{
		num=-num;
		LCD_DisplayStringLine(Line,Column,"-");
		len++;
	}
	Temp=(uint64_t)(num*100000);
	
	Temp1=(uint32_t)(Temp/100000);
	
	LCD_DisplayNum(Line,Column+Column(len),Temp1);
	
	do
	{
		Temp1 = Temp1/10;
		len++;
	}
	while(Temp1);
	
	Temp1=(uint32_t)(Temp%100000);
	
	while(Temp1%10==0)
		Temp1=Temp1/10;

	if(Temp1!=0)
	{
		LCD_DisplayStringLine(Line,Column+Column(len),".");
		len++;
		LCD_DisplayNum(Line,Column+Column(len),Temp1);
	}
}
#endif

#if _USE_GRAPH








void PutPixel(int16_t x, int16_t y)
{
	LCD_SetCursor(x,y);
	LCD_WriteRAM_Prepare();
	LCD_WriteRAM(TextColor);
}







void LCD_WeBMP_SIZE(uint16_t *ptr,uint32_t SIZE)
{
	uint32_t index;
	LCD_WriteRAM_Prepare(); 
	for (index = 0; index < SIZE; index++)
		LCD_WriteRAM(*ptr++);
}










void LCD_DrawLine(uint16_t Xpos, uint16_t Ypos, uint16_t Length, uint8_t Direction)
{
	uint32_t i = 0;
	LCD_SetCursor(Xpos, Ypos);
	if(Direction == 0)
	{
		LCD_WriteRAM_Prepare(); 
		for(i = 0; i < Length; i++)
			LCD_WriteRAM(TextColor);
	}
	else
	{
		for(i = 0; i < Length; i++)
		{
			LCD_WriteRAM_Prepare(); 
			LCD_WriteRAM(TextColor);
			Ypos++;
			LCD_SetCursor(Xpos, Ypos);
		}
	}
}










#define ABS(X)  ((X) > 0 ? (X) : -(X))

void LCD_DrawuniLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
	int16_t deltax = 0, deltay = 0, x = 0, y = 0, xinc1 = 0, xinc2 = 0,
	        yinc1 = 0, yinc2 = 0, den = 0, num = 0, numadd = 0, numpixels = 0,
	        curpixel = 0;
	deltax = ABS(x2 - x1);        
	deltay = ABS(y2 - y1);        
	x = x1;                       
	y = y1;                       
	if (x2 >= x1)                 
	{
		xinc1 = 1;
		xinc2 = 1;
	}
	else                          
	{
		xinc1 = -1;
		xinc2 = -1;
	}
	if (y2 >= y1)                 
	{
		yinc1 = 1;
		yinc2 = 1;
	}
	else                          
	{
		yinc1 = -1;
		yinc2 = -1;
	}
	if (deltax >= deltay)         
	{
		xinc1 = 0;                  
		yinc2 = 0;                  
		den = deltax;
		num = deltax / 2;
		numadd = deltay;
		numpixels = deltax;         
	}
	else                          
	{
		xinc2 = 0;                  
		yinc1 = 0;                  
		den = deltay;
		num = deltay / 2;
		numadd = deltax;
		numpixels = deltay;         
	}
	for (curpixel = 0; curpixel <= numpixels; curpixel++)
	{
		PutPixel(x, y);             
		num += numadd;              
		if (num >= den)             
		{
			num -= den;               
			x += xinc1;               
			y += yinc1;               
		}
		x += xinc2;                 
		y += yinc2;                 
	}
}









void LCD_DrawRect(uint16_t Xpos, uint16_t Ypos, uint16_t Height, uint16_t Width)
{
	LCD_DrawLine(Xpos,      Ypos,       Width,0);
	LCD_DrawLine(Xpos,      Ypos+Height,Width,0);
	LCD_DrawLine(Xpos,      Ypos,       Height,1);
	LCD_DrawLine(Xpos+Width,Ypos,       Height,1);
}









void LCD_DrawRectS(u16 x1,u16 y1,u16 x2,u16 y2,u16 color)
{
	u16 temp=TextColor;
	TextColor=color;
	for(; x2>=x1; x2--)
		LCD_DrawuniLine(x2,y1,x2,y2);
	TextColor=temp;
}







void LCD_DrawCircle(uint16_t Xpos, uint16_t Ypos, uint16_t Radius)
{
	int32_t  D;
	uint32_t  CurX;
	uint32_t  CurY;
	D = 3 - (Radius << 1);
	CurX = 0;
	CurY = Radius;
	while (CurX <= CurY)
	{
		LCD_SetCursor(Xpos + CurX, Ypos + CurY);
		LCD_WriteRAM_Prepare(); 
		LCD_WriteRAM(TextColor);
		LCD_SetCursor(Xpos + CurX, Ypos - CurY);
		LCD_WriteRAM_Prepare(); 
		LCD_WriteRAM(TextColor);
		LCD_SetCursor(Xpos - CurX, Ypos + CurY);
		LCD_WriteRAM_Prepare(); 
		LCD_WriteRAM(TextColor);
		LCD_SetCursor(Xpos - CurX, Ypos - CurY);
		LCD_WriteRAM_Prepare(); 
		LCD_WriteRAM(TextColor);
		LCD_SetCursor(Xpos + CurY, Ypos + CurX);
		LCD_WriteRAM_Prepare(); 
		LCD_WriteRAM(TextColor);
		LCD_SetCursor(Xpos + CurY, Ypos - CurX);
		LCD_WriteRAM_Prepare(); 
		LCD_WriteRAM(TextColor);
		LCD_SetCursor(Xpos - CurY, Ypos + CurX);
		LCD_WriteRAM_Prepare(); 
		LCD_WriteRAM(TextColor);
		LCD_SetCursor(Xpos - CurY, Ypos - CurX);
		LCD_WriteRAM_Prepare(); 
		LCD_WriteRAM(TextColor);
		if (D < 0)
			D += (CurX << 2) + 6;
		else
		{
			D += ((CurX - CurY) << 2) + 10;
			CurY--;
		}
		CurX++;
	}
}







void LCD_DrawCircleS(uint16_t x,uint16_t y,uint16_t size,uint16_t color)
{
	uint16_t temp=TextColor;
	TextColor =color;
	for(; size>0; size--)
		LCD_DrawCircle(x,y,size);
	TextColor=temp;
}

#endif

#if __TFT_ID==0x1936







void LCD_DrawPoint_4(uint16_t X,uint16_t Y,uint16_t Col)
{
	LCD_REG_Select(0x002A);
	LCD_WriteRAM(X>>8);
	LCD_WriteRAM(X&0x00ff);
	LCD_WriteRAM(0x03);
	LCD_WriteRAM(0x1F);
	LCD_REG_Select(0x002b);
	LCD_WriteRAM(Y>>8);
	LCD_WriteRAM(Y&0x00ff);
	LCD_WriteRAM(0x01);
	LCD_WriteRAM(0xdf);
	LCD_REG_Select(0x002c);
	LCD->LCD_RAM = Col;
	LCD->LCD_RAM = Col;
	Y++;
	LCD_REG_Select(0x002A);
	LCD_WriteRAM(X>>8);
	LCD_WriteRAM(X&0x00ff);
	LCD_WriteRAM(0x03);
	LCD_WriteRAM(0x1F);
	LCD_REG_Select(0x002b);
	LCD_WriteRAM(Y>>8);
	LCD_WriteRAM(Y&0x00ff);
	LCD_WriteRAM(0x01);
	LCD_WriteRAM(0xdf);
	LCD_REG_Select(0x002c);
	LCD->LCD_RAM = Col;
	LCD->LCD_RAM = Col;
}







static void TftLcdDevice_DrawPoint(TftLcdDevice *self, uint16_t x, uint16_t y, uint16_t color)
{
	(void)self;
	LCD_REG_Select(0x002A);
	LCD_WriteRAM(x>>8);
	LCD_WriteRAM(x&0x00ff);
	LCD_WriteRAM(0x03);
	LCD_WriteRAM(0x1F);
	LCD_REG_Select(0x002b);
	LCD_WriteRAM(y>>8);
	LCD_WriteRAM(y&0x00ff);
	LCD_WriteRAM(0x01);
	LCD_WriteRAM(0xdf);
	LCD_REG_Select(0x002c);
	LCD->LCD_RAM = color;
}

void LCD_DrawPoint(uint16_t X,uint16_t Y,uint16_t Col)
{
	TftLcdDevice_Construct(&g_tft_lcd);
	g_tft_lcd.ops->draw_point(&g_tft_lcd, X, Y, Col);
}
#else
static void TftLcdDevice_DrawPoint(TftLcdDevice *self, uint16_t x, uint16_t y, uint16_t color)
{
	(void)self;
	LCD->LCD_REG = LCD_REG_32;
	
	LCD->LCD_RAM = x;
	
	LCD->LCD_REG = LCD_REG_33;
	
	LCD->LCD_RAM = y;
	LCD->LCD_REG = LCD_REG_34;
	LCD->LCD_RAM = color;
}

void LCD_DrawPoint(uint16_t X,uint16_t Y,uint16_t Col)
{
	TftLcdDevice_Construct(&g_tft_lcd);
	g_tft_lcd.ops->draw_point(&g_tft_lcd, X, Y, Col);
}
#endif

#if __TFT_ID==0x1936







void LCD_SetDisplayWindow(uint16_t SC,uint16_t SP,uint16_t EC,uint16_t EP)
{
	LCD_REG_Select(0x002A);
	LCD_WriteRAM(SC>>8);
	LCD_WriteRAM(SC&0x00ff);
	LCD_WriteRAM((uint8_t)(EC>>8));
	LCD_WriteRAM((uint8_t)(EC&0x00ff));
	LCD_REG_Select(0x002b);
	LCD_WriteRAM(SP>>8);
	LCD_WriteRAM(SP&0x00ff);
	LCD_WriteRAM((uint8_t)(EP>>8));
	LCD_WriteRAM((uint8_t)(EP&0x00ff));
}
#endif

#if __TFT_ID==0x1936







void LCD_SetCursor(uint16_t Xpos, uint16_t Ypos)
{
	LCD_REG_Select(0x002A);
	LCD_WriteRAM(Xpos>>8);
	LCD_WriteRAM(Xpos&0x00ff);
	LCD_WriteRAM(0x03);
	LCD_WriteRAM(0x1F);
	LCD_REG_Select(0x002b);
	LCD_WriteRAM(Ypos>>8);
	LCD_WriteRAM(Ypos&0x00ff);
	LCD_WriteRAM(0x01);
	LCD_WriteRAM(0xdf);
}
#else







void LCD_SetCursor(uint16_t Xpos, uint8_t Ypos)
{



	LCD_WriteReg(LCD_REG_32,240-Ypos);
	LCD_WriteReg(LCD_REG_33,Xpos);
}
#endif

#if _USE_DisplayOn





void LCD_DisplayOn(void)
{
	/* PD3 控制 LCD 背光。 */
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_3, GPIO_PIN_SET);
}





void LCD_DisplayOff(void)
{
	/* PD3 控制 LCD 背光。 */
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_3, GPIO_PIN_RESET);
}
#endif







static void TftLcdDevice_WriteReg(TftLcdDevice *self, uint16_t reg, uint16_t value)
{
	(void)self;
	LCD->LCD_REG = reg;
	LCD->LCD_RAM = value;
}

void LCD_WriteReg(uint16_t LCD_Reg, uint16_t LCD_RegValue)
{
	TftLcdDevice_Construct(&g_tft_lcd);
	g_tft_lcd.ops->write_reg(&g_tft_lcd, LCD_Reg, LCD_RegValue);
}






static uint16_t TftLcdDevice_ReadReg(TftLcdDevice *self, uint16_t reg)
{
	(void)self;
	LCD->LCD_REG = reg;
	return (LCD->LCD_RAM);
}

uint16_t LCD_ReadReg(uint16_t LCD_Reg)
{
	TftLcdDevice_Construct(&g_tft_lcd);
	return g_tft_lcd.ops->read_reg(&g_tft_lcd, LCD_Reg);
}





void LCD_WriteRAM_Prepare(void)
{
#if __TFT_ID==0x1936
	LCD_REG_Select(0x002c);
#else
	LCD->LCD_REG = LCD_REG_34;
#endif
}






void LCD_REG_Select(uint16_t REG_Code)
{
	LCD->LCD_REG = REG_Code;
}






void LCD_WriteRAM(uint16_t RGB_Code)
{
	LCD->LCD_RAM = RGB_Code;
}






void LCD_CtrlLinesConfig(void)
{
	GPIO_InitTypeDef gpio_init_struct;

	/* FSMC 数据线、控制线和背光 GPIO 时钟。 */
	__HAL_RCC_GPIOD_CLK_ENABLE();
	__HAL_RCC_GPIOE_CLK_ENABLE();
	__HAL_RCC_GPIOF_CLK_ENABLE();
	__HAL_RCC_GPIOG_CLK_ENABLE();

	gpio_init_struct.Mode = GPIO_MODE_AF_PP;
	gpio_init_struct.Pull = GPIO_NOPULL;
	gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
	gpio_init_struct.Alternate = GPIO_AF12_FSMC;

	/* FSMC 数据线和读写控制线。 */
	gpio_init_struct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5 |
	                       GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
	                       GPIO_PIN_14 | GPIO_PIN_15;
	HAL_GPIO_Init(GPIOD, &gpio_init_struct);

	/* FSMC 高位数据线。 */
	gpio_init_struct.Pin = GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
	                       GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 |
	                       GPIO_PIN_14 | GPIO_PIN_15;
	HAL_GPIO_Init(GPIOE, &gpio_init_struct);

	/* PF0 作为 LCD 寄存器/数据选择地址线。 */
	gpio_init_struct.Pin = GPIO_PIN_0;
	HAL_GPIO_Init(GPIOF, &gpio_init_struct);

	/* LCD 背光输出。 */
	gpio_init_struct.Pin = GPIO_PIN_3;
	gpio_init_struct.Mode = GPIO_MODE_OUTPUT_PP;
	gpio_init_struct.Pull = GPIO_NOPULL;
	gpio_init_struct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOD, &gpio_init_struct);
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_3, GPIO_PIN_SET);
}






void LCD_FSMCConfig(void)
{
	FSMC_NORSRAM_TimingTypeDef timing;

	/* 通过 HAL SRAM 驱动配置 FSMC Bank1，供 LCD 16 位并口访问。 */
	__HAL_RCC_FSMC_CLK_ENABLE();

	g_lcd_sram_handler.Instance = FSMC_NORSRAM_DEVICE;
	g_lcd_sram_handler.Extended = FSMC_NORSRAM_EXTENDED_DEVICE;
	g_lcd_sram_handler.Init.NSBank = FSMC_NORSRAM_BANK1;
	g_lcd_sram_handler.Init.DataAddressMux = FSMC_DATA_ADDRESS_MUX_DISABLE;
	g_lcd_sram_handler.Init.MemoryType = FSMC_MEMORY_TYPE_SRAM;
	g_lcd_sram_handler.Init.MemoryDataWidth = FSMC_NORSRAM_MEM_BUS_WIDTH_16;
	g_lcd_sram_handler.Init.BurstAccessMode = FSMC_BURST_ACCESS_MODE_DISABLE;
	g_lcd_sram_handler.Init.WaitSignalPolarity = FSMC_WAIT_SIGNAL_POLARITY_LOW;
	g_lcd_sram_handler.Init.WaitSignalActive = FSMC_WAIT_TIMING_BEFORE_WS;
	g_lcd_sram_handler.Init.WriteOperation = FSMC_WRITE_OPERATION_ENABLE;
	g_lcd_sram_handler.Init.WaitSignal = FSMC_WAIT_SIGNAL_DISABLE;
	g_lcd_sram_handler.Init.ExtendedMode = FSMC_EXTENDED_MODE_DISABLE;
	g_lcd_sram_handler.Init.AsynchronousWait = FSMC_ASYNCHRONOUS_WAIT_DISABLE;
	g_lcd_sram_handler.Init.WriteBurst = FSMC_WRITE_BURST_DISABLE;
	g_lcd_sram_handler.Init.ContinuousClock = FSMC_CONTINUOUS_CLOCK_SYNC_ONLY;
	g_lcd_sram_handler.Init.PageSize = FSMC_PAGE_SIZE_NONE;

	timing.AddressSetupTime = 4;
	timing.AddressHoldTime = 0;
	timing.DataSetupTime = 8;
	timing.BusTurnAroundDuration = 0;
	timing.CLKDivision = 0;
	timing.DataLatency = 0;
	timing.AccessMode = FSMC_ACCESS_MODE_A;

	HAL_SRAM_Init(&g_lcd_sram_handler, &timing, &timing);
}
