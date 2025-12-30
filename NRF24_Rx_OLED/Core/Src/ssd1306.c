#include "ssd1306.h"


// Screenbuffer
static uint8_t SSD1306_Buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8];

// Screen object
static SSD1306_t SSD1306;


//
//  Send a byte to the command register
//
static uint8_t ssd1306_WriteCommand(I2C_HandleTypeDef *hi2c, uint8_t command)
{
    return HAL_I2C_Mem_Write(hi2c, SSD1306_I2C_ADDR, 0x00, 1, &command, 1, 10);
}


//
//  Initialize the oled screen
//
// PODMIEŃ CAŁĄ TĘ FUNKCJĘ W PLIKU ssd1306.c

uint8_t ssd1306_Init(I2C_HandleTypeDef *hi2c)
{
    HAL_Delay(100); // Czekamy na start ekranu

    int status = 0;

    // --- SEKWENCJA INICJALIZACJA DLA 128x32 (0.91") ---

    status += ssd1306_WriteCommand(hi2c, 0xAE); // 1. Display OFF

    status += ssd1306_WriteCommand(hi2c, 0xA8); // 2. Set Multiplex Ratio
    status += ssd1306_WriteCommand(hi2c, 0x1F); //    0x1F = 32 linie (dla 128x32)

    status += ssd1306_WriteCommand(hi2c, 0xD3); // 3. Set Display Offset
    status += ssd1306_WriteCommand(hi2c, 0x00); //    0 offset

    status += ssd1306_WriteCommand(hi2c, 0x40); // 4. Set Display Start Line (0)

    status += ssd1306_WriteCommand(hi2c, 0xA1); // 5. Set Segment Re-map (Obraca obraz w poziomie)
    // Jeśli obraz będzie lustrzanym odbiciem, zmień 0xA1 na 0xA0

    status += ssd1306_WriteCommand(hi2c, 0xC8); // 6. Set COM Output Scan Direction (Obraca w pionie)
    // Jeśli obraz będzie "do góry nogami", zmień 0xC8 na 0xC0

    status += ssd1306_WriteCommand(hi2c, 0xDA); // 7. Set COM Pins Hardware Configuration
    status += ssd1306_WriteCommand(hi2c, 0x02); //    WAŻNE! 0x02 = Sequential, No Remap (Klucz dla 128x32)

    status += ssd1306_WriteCommand(hi2c, 0x81); // 8. Set Contrast Control
    status += ssd1306_WriteCommand(hi2c, 0x7F); //    Średni kontrast (0x00-0xFF)

    status += ssd1306_WriteCommand(hi2c, 0xA4); // 9. Entire Display ON (resume)

    status += ssd1306_WriteCommand(hi2c, 0xA6); // 10. Set Normal Display (0xA7 = inwersja)

    status += ssd1306_WriteCommand(hi2c, 0xD5); // 11. Set Osc Frequency
    status += ssd1306_WriteCommand(hi2c, 0x80);

    status += ssd1306_WriteCommand(hi2c, 0x8D); // 12. Charge Pump Setting
    status += ssd1306_WriteCommand(hi2c, 0x14); //     Enable Charge Pump (bez tego ekran jest czarny!)

    status += ssd1306_WriteCommand(hi2c, 0xAF); // 13. Display ON

    if (status != 0) {
        return 1;
    }

    // Wyczyść bufor i ekran na start
    ssd1306_Fill(Black);
    ssd1306_UpdateScreen(hi2c);

    // Reset zmiennych struktury
    SSD1306.CurrentX = 0;
    SSD1306.CurrentY = 0;
    SSD1306.Initialized = 1;

    return 0;
}
//
//  Fill the whole screen with the given color
//
void ssd1306_Fill(SSD1306_COLOR color)
{
    // Fill screenbuffer with a constant value (color)
    uint32_t i;

    for(i = 0; i < sizeof(SSD1306_Buffer); i++)
    {
        SSD1306_Buffer[i] = (color == Black) ? 0x00 : 0xFF;
    }
}

//
//  Write the screenbuffer with changed to the screen
//
void ssd1306_UpdateScreen(I2C_HandleTypeDef *hi2c)
{
    uint8_t i;

    for (i = 0; i < 8; i++) {
        ssd1306_WriteCommand(hi2c, 0xB0 + i);
        ssd1306_WriteCommand(hi2c, 0x00);
        ssd1306_WriteCommand(hi2c, 0x10);

        HAL_I2C_Mem_Write(hi2c, SSD1306_I2C_ADDR, 0x40, 1, &SSD1306_Buffer[SSD1306_WIDTH * i], SSD1306_WIDTH, 100);
    }
}

//
//  Draw one pixel in the screenbuffer
//  X => X Coordinate
//  Y => Y Coordinate
//  color => Pixel color
//
void ssd1306_DrawPixel(uint8_t x, uint8_t y, SSD1306_COLOR color)
{
    if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT)
    {
        // Don't write outside the buffer
        return;
    }

    // Check if pixel should be inverted
    if (SSD1306.Inverted)
    {
        color = (SSD1306_COLOR)!color;
    }

    // Draw in the correct color
    if (color == White)
    {
        SSD1306_Buffer[x + (y / 8) * SSD1306_WIDTH] |= 1 << (y % 8);
    }
    else
    {
        SSD1306_Buffer[x + (y / 8) * SSD1306_WIDTH] &= ~(1 << (y % 8));
    }
}


//
//  Draw 1 char to the screen buffer
//  ch      => Character to write
//  Font    => Font to use
//  color   => Black or White
//
char ssd1306_WriteChar(char ch, FontDef Font, SSD1306_COLOR color)
{
    uint32_t i, b, j;

    // Check remaining space on current line
    if (SSD1306_WIDTH <= (SSD1306.CurrentX + Font.FontWidth) ||
        SSD1306_HEIGHT <= (SSD1306.CurrentY + Font.FontHeight))
    {
        // Not enough space on current line
        return 0;
    }

    // Translate font to screenbuffer
    for (i = 0; i < Font.FontHeight; i++)
    {
        b = Font.data[(ch - 32) * Font.FontHeight + i];
        for (j = 0; j < Font.FontWidth; j++)
        {
            if ((b << j) & 0x8000)
            {
                ssd1306_DrawPixel(SSD1306.CurrentX + j, (SSD1306.CurrentY + i), (SSD1306_COLOR) color);
            }
            else
            {
                ssd1306_DrawPixel(SSD1306.CurrentX + j, (SSD1306.CurrentY + i), (SSD1306_COLOR)!color);
            }
        }
    }

    // The current space is now taken
    SSD1306.CurrentX += Font.FontWidth;

    // Return written char for validation
    return ch;
}

//
//  Write full string to screenbuffer
//
char ssd1306_WriteString(const char* str, FontDef Font, SSD1306_COLOR color)
{
    // Write until null-byte
    while (*str)
    {
        if (ssd1306_WriteChar(*str, Font, color) != *str)
        {
            // Char could not be written
            return *str;
        }

        // Next char
        str++;
    }

    // Everything ok
    return *str;
}

//
//  Invert background/foreground colors
//
void ssd1306_InvertColors(void)
{
    SSD1306.Inverted = !SSD1306.Inverted;
}

//
//  Set cursor position
//
void ssd1306_SetCursor(uint8_t x, uint8_t y)
{
    SSD1306.CurrentX = x;
    SSD1306.CurrentY = y;
}
