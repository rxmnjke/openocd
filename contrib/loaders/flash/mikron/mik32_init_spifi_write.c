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
  // ждём завершения отработки команды - в данном случае команда завершится, когда контроллер SPIFI
  // прочитает регистр состояния с указанным состоянием бита
  wait_SPIFI_CMD();
  // прочитаем один байт из FIFO - это собственно содержимое регистра состояния
  SPIFI_CONFIG->DATA8;
}


// ожидать завершения сброса контроллера SPIFI
void wait_SPIFI_RESET() {
  // проверяем бит RESET, который сбрасывается контроллером после завршения сброса
  while ( 0 != (SPIFI_CONFIG->STAT & SPIFI_CONFIG_STAT_RESET_M) ) {}
}


// по указателю a_result будет сложен JEDEC ID, или 0 (ноль) если не прочиталось
__attribute__((used))
void main( uint32_t * a_result ) {
  // сброс результата
  *a_result = 0;
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
  // отключение запросов к DMA и максимальный интервал между командами
  SPIFI_CONFIG->CTRL = (SPIFI_CONFIG->CTRL & ~SPIFI_CONFIG_CTRL_DMAEN_M)
                     | SPIFI_CONFIG_CTRL_CSHIGH_M
                     | (2 << SPIFI_CONFIG_CTRL_SCK_DIV_S)
                     ;

  // типа настройка Fast Read Quad I/O на работу с передачей адреса
  // (если SPI FLASH был в однопроводном режиме, просто ничего не произойдёт)
  SPIFI_CONFIG->STAT |= SPIFI_CONFIG_STAT_INTRQ_M;
  SPIFI_CONFIG->ADDR = 0;
  SPIFI_CONFIG->IDATA = 0x0; // содержимое первого dummy байта команды Fast Read Quad I/O (EBh)
  SPIFI_CONFIG->CMD = (1 << SPIFI_CONFIG_CMD_DATALEN_S) // прочитаем один байт
                    | (1 << SPIFI_CONFIG_MCMD_INTLEN_S) // один dummy байт 0x0
                    | (3 << SPIFI_CONFIG_MCMD_FIELDFORM_S) // всё по четырём
                    | (6 << SPIFI_CONFIG_MCMD_FRAMEFORM_S) // без кода команды, три байта адреса
                    | (0xEB << SPIFI_CONFIG_MCMD_OPCODE_S)
                    ;
  wait_start_SPIFI_CMD();
  // читаем один байт
  SPIFI_CONFIG->DATA8;
  // ожидаем завершения команды контроллером SPIFI
  wait_SPIFI_CMD();
  // выполняем команду Disable QPI (FFh) (передача команд по одной линии)
  // (если SPI FLASH был в однопроводном режиме, просто ничего не произойдёт)
  SPIFI_CONFIG->STAT |= SPIFI_CONFIG_STAT_INTRQ_M;
  SPIFI_CONFIG->CMD = (1 << SPIFI_CONFIG_CMD_FRAMEFORM_S) // отправляем только код команды
                    | (3 << SPIFI_CONFIG_MCMD_FIELDFORM_S) // всё по четырём
                    | (0xFF << SPIFI_CONFIG_CMD_OPCODE_S)
                    ;
  wait_start_SPIFI_CMD();
  // ожидаем завершения команды контроллером SPIFI
  wait_SPIFI_CMD();
  // выполняем команду Read Status Register-2 (35h)
  SPIFI_CONFIG->STAT |= SPIFI_CONFIG_STAT_INTRQ_M;
  SPIFI_CONFIG->CMD = (1 << SPIFI_CONFIG_CMD_DATALEN_S) // читаем 1 байт
                    | (1 << SPIFI_CONFIG_CMD_FRAMEFORM_S) // отправляем только код команды
                    | (0x35 << SPIFI_CONFIG_CMD_OPCODE_S)
                    ;
  wait_start_SPIFI_CMD();
  // выбираем прочитанное
  uint8_t v_sr2 = SPIFI_CONFIG->DATA8;
  // ожидаем завершения команды контроллером SPIFI
  wait_SPIFI_CMD();
  // если бит QUAD ENABLE не установлен
  if ( 0 == (v_sr2 & STATUS_REGISTER_QE) ) {
    // будем его устанавливать
    // сначала выполняем команду Volatile Write Enable (50h) (запись бита только до отключения питания)
    SPIFI_CONFIG->STAT |= SPIFI_CONFIG_STAT_INTRQ_M;
    SPIFI_CONFIG->CMD = (1 << SPIFI_CONFIG_CMD_FRAMEFORM_S) // отправляем только код команды
                      | (0x50 << SPIFI_CONFIG_CMD_OPCODE_S)
                      ;
    wait_start_SPIFI_CMD();
    // ожидаем завершения команды контроллером SPIFI
    wait_SPIFI_CMD();
    // потом команду Write Status Register-2 (31h)
    SPIFI_CONFIG->STAT |= SPIFI_CONFIG_STAT_INTRQ_M;
    SPIFI_CONFIG->CMD = (1 << SPIFI_CONFIG_CMD_DATALEN_S) // отправляем 1 байт
                      | SPIFI_CONFIG_CMD_DOUT_M // выдача данных контроллером в SPI FLASH
                      | (1 << SPIFI_CONFIG_CMD_FRAMEFORM_S) // отправляем только код команды, без адреса
                      | (0x31 << SPIFI_CONFIG_CMD_OPCODE_S)
                      ;
    wait_start_SPIFI_CMD();
    // SR2 с взведённым битом Quad Enable
    SPIFI_CONFIG->DATA8 = v_sr2 | STATUS_REGISTER_QE;
    // ожидаем завершения команды контроллером SPIFI
    wait_SPIFI_CMD();
    // ожидаем завершения записи со стороны SPI FLASH
    wait_for_flash();
  }
  // прочитаем JEDEC ID
  SPIFI_CONFIG->STAT |= SPIFI_CONFIG_STAT_INTRQ_M;
  SPIFI_CONFIG->CMD = (3 << SPIFI_CONFIG_CMD_DATALEN_S) // читаем 3 байта
                    | (1 << SPIFI_CONFIG_CMD_FRAMEFORM_S) // отправляем только код команды
                    | (0x9F << SPIFI_CONFIG_CMD_OPCODE_S)
                    ;
  wait_start_SPIFI_CMD();
  // читаем три байта
  uint32_t v_u32 = SPIFI_CONFIG->DATA8;
  v_u32 |= SPIFI_CONFIG->DATA8 << 8;
  v_u32 |= SPIFI_CONFIG->DATA8 << 16;
  // ожидаем завершения команды контроллером SPIFI
  wait_SPIFI_CMD();
  //
  *a_result = v_u32;
}
