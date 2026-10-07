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


void wait_start_SPIFI_CMD() {
  while ( 0 == (SPIFI_CONFIG->STAT & SPIFI_CONFIG_STAT_CMD_M) ) {}
}


// ожидание завершения отработки команды контроллером SPIFI
void wait_SPIFI_CMD() {
  // проверяем бит INTRQ, который выставляется контроллером по завершении команды
  while ( 0 != (SPIFI_CONFIG->STAT & SPIFI_CONFIG_STAT_CMD_M) ) {}
}


// ожидать завершения сброса контроллера SPIFI
void wait_SPIFI_RESET() {
  // проверяем бит RESET, который сбрасывается контроллером после завршения сброса
  while ( 0 != (SPIFI_CONFIG->STAT & SPIFI_CONFIG_STAT_RESET_M) ) {}
}


// ожидание завершения текущей операции записи/стирания, которую обрабатывает SPI FLASH
void wait_for_flash() {
  // ждём завершения операции (выполняемем Read Status Register-1 (05h), пока нулевой бит не станет равным нулю)
  SPIFI_CONFIG->STAT |= SPIFI_CONFIG_STAT_INTRQ_M;
  SPIFI_CONFIG->CTRL &= ~SPIFI_CONFIG_CTRL_DMAEN_M;
  SPIFI_CONFIG->CMD = SPIFI_CONFIG_CMD_POLL_INDEX(1) // номер отслеживаемого бита 0 (ноль), SR1.BUSY
                    | SPIFI_CONFIG_CMD_POLL_REQUIRED_VALUE(0) // требуемое состояние отслеживаемого бита - 0 (ноль)
                    | SPIFI_CONFIG_CMD_POLL_M // режим опроса (повторение чтения до получения требуемого состояни указанного бита)
                    | (1 << SPIFI_CONFIG_CMD_FRAMEFORM_S) // отправляем только код команды
                    | (0x05 << SPIFI_CONFIG_CMD_OPCODE_S)
                    ;
  wait_start_SPIFI_CMD();
  // ждём завершения отработки команды - в данном случае команда завершится, когда контроллер SPIFI
  // прочитает регистр состояния с указанным состоянием бита
  wait_SPIFI_CMD();
  // прочитаем один байт из FIFO - это собственно содержимое регистра состояния
  SPIFI_CONFIG->DATA8;
}


// стирание всего флэша
__attribute__((used))
void main(void) {
  // сброс контроллера
  SPIFI_CONFIG->STAT = SPIFI_CONFIG_STAT_RESET_M;
  //
  wait_SPIFI_RESET();
  // сначала выполняем команду Write Enable (06h)
  SPIFI_CONFIG->STAT |= SPIFI_CONFIG_STAT_INTRQ_M;
  SPIFI_CONFIG->CMD = (1 << SPIFI_CONFIG_CMD_FRAMEFORM_S) // отправляем только код команды
                    | (0x06 << SPIFI_CONFIG_CMD_OPCODE_S)
                    ;
  wait_start_SPIFI_CMD();
  // ожидаем завершения команды контроллером SPIFI
  wait_SPIFI_CMD();
  //
  // Chip Erase (60h/C7h) стирание всего флэша
  SPIFI_CONFIG->STAT |= SPIFI_CONFIG_STAT_INTRQ_M;
  SPIFI_CONFIG->CMD = (1 << SPIFI_CONFIG_CMD_FRAMEFORM_S) // отправляем код команды
                    | (0x60 << SPIFI_CONFIG_CMD_OPCODE_S)
                    ;
  wait_start_SPIFI_CMD();
  // ожидаем завершения команды контроллером SPIFI
  wait_SPIFI_CMD();
  // ожидаем завершения стирания сектора от SPI FLASH
  wait_for_flash();
}
