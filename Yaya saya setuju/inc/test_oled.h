#ifndef TEST_OLED_H
#define TEST_OLED_H

/* Rutin test OLED SH1106 (firmware HAL, bukan Arduino).
 * Panggil dari main.c:
 *   - USER CODE BEGIN 2 -> TestOled_Init();
 *   - USER CODE BEGIN 3 (while(1)) -> TestOled_Run();
 */
void TestOled_Init(void);
void TestOled_Run(void);

#endif /* TEST_OLED_H */
