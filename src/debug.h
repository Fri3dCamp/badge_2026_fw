#ifndef __DEBUG_H
#define __DEBUG_H

#include <stdio.h>
#include <ch32x035.h> /* both X033 and X035 */

#ifdef __cplusplus
extern "C"
{
#endif

  void Delay_Init(void);
  void Delay_Us(uint32_t n);
  void Delay_Ms(uint32_t n);
  void USART4_Printf_Init(uint32_t baudrate);

#if (DEBUG)
#define PRINT(format, ...) printf(format, ##__VA_ARGS__)
#else
#define PRINT(X...)
#endif

#ifdef __cplusplus
}
#endif

#endif
