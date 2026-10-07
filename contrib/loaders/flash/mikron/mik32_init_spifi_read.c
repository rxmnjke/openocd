#include "mik32_hwlibs/power_manager.h"
#include "mik32_hwlibs/wakeup.h"
#include "mik32_hwlibs/gpio.h"
#include "mik32_hwlibs/pad_config.h"
#include "mik32_hwlibs/csr.h"
#include "mik32_hwlibs/scr1_csr_encoding.h"
#include "mik32_hwlibs/scr1_timer.h"
#include "mik32_hwlibs/uart.h"
#include "mik32_hwlibs/spifi.h"
#include "mik32_hwlibs/crc.h"
#include "mik32_hwlibs/dma_config.h"
#include "mik32_hwlibs/mik32_memory_map.h"


#include <stdint.h>
#include <stddef.h>


#define FLASH_SECTOR_SIZE   0x00001000
#define FLASH_PAGE_SIZE     256
#define FLASH_ADDR_BASE     0x80000000
#define FLASH_MAIN_SIZE     0x00800000

#define STATUS_REGISTER_QE (1<<1)


// ожидать завершения сброса контроллера SPIFI
void wait_SPIFI_RESET() {
  // проверяем бит RESET, который сбрасывается контроллером после завршения сброса
  while ( 0 != (SPIFI_CONFIG->STAT & SPIFI_CONFIG_STAT_RESET_M) ) {}
}


// активация XIP
__attribute__((used))
void main( uint32_t * a_data_addr, uint32_t a_in_flash_addr ) {
  // включаем тактирование GPIO_2, PAD_CONFIG, SPIFI, DMA
  PM->CLK_APB_M_SET = PM_CLOCK_APB_M_PAD_CONFIG_M;
  PM->CLK_APB_P_SET = PM_CLOCK_APB_P_GPIO_2_M;
  PM->CLK_AHB_SET = PM_CLOCK_AHB_SPIFI_M | PM_CLOCK_AHB_DMA_M;
  // PORT2.0-PORT2.5 - SPIFI (sck, cs, D0, D1, D2, D3)
  PAD_CONFIG->PORT_2_CFG = (PAD_CONFIG->PORT_2_CFG & ~(0xFFF))
                         | PAD_CONFIG_PIN(0, 1)
                         | PAD_CONFIG_PIN(1, 1)
                         | PAD_CONFIG_PIN(2, 1)
                         | PAD_CONFIG_PIN(3, 1)
                         | PAD_CONFIG_PIN(4, 1)
                         | PAD_CONFIG_PIN(5, 1)
                         ;
  PAD_CONFIG->PORT_2_DS = (PAD_CONFIG->PORT_2_DS & ~(0xFFF));
  PAD_CONFIG->PORT_2_PUPD = (PAD_CONFIG->PORT_2_PUPD & ~(0xFFF))
                          | PAD_CONFIG_PIN(0, 2) // SCK к общему проводу
                          | PAD_CONFIG_PIN(1, 1) // CS и линии данных - к питанию
                          | PAD_CONFIG_PIN(2, 1)
                          | PAD_CONFIG_PIN(3, 1)
                          | PAD_CONFIG_PIN(4, 1)
                          | PAD_CONFIG_PIN(5, 1)
                          ;
  // сброс контроллера
  SPIFI_CONFIG->STAT = SPIFI_CONFIG_STAT_RESET_M;
  //
  wait_SPIFI_RESET();
  // настроим SPIFI на работу "с памятью"
  SPIFI_CONFIG->CTRL = (SPIFI_CONFIG->CTRL & ~(SPIFI_CONFIG_CTRL_CSHIGH_M))
                     | (0 << SPIFI_CONFIG_CTRL_CSHIGH_S) // 1 такт сигнала SCK между командами
                     | SPIFI_CONFIG_CTRL_CACHE_EN_M // включение кэширования
                     | SPIFI_CONFIG_CTRL_D_CACHE_DIS_M // отключение кэширования данных
                     ;
  // минимальный делитель
  SPIFI_CONFIG->CTRL &= ~SPIFI_CONFIG_CTRL_SCK_DIV_M;
  //
  SPIFI_CONFIG->ADDR = 0;
  SPIFI_CONFIG->CLIMIT = FLASH_ADDR_BASE + FLASH_MAIN_SIZE; // граница кэширования
  SPIFI_CONFIG->MCMD = (4 << SPIFI_CONFIG_MCMD_FRAMEFORM_S) // код команды и три байта адреса
                     | (0x03 << SPIFI_CONFIG_MCMD_OPCODE_S)
                     ;
}
