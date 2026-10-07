#include "mik32_hwlibs/eeprom.h"
#include "mik32_hwlibs/mik32_memory_map.h"

#include <stdint.h>
#include <stddef.h>


#define EEPROM_SECTOR_SIZE   128
#define EEPROM_PAGE_SIZE     128
#define EEPROM_ADDR_BASE     0x01000000
#define EEPROM_MAIN_SIZE     0x00002000


// стирание сектора EEPROM (128 байтов) или всех секторов
__attribute__((used))
void main( uint32_t a_in_eeprom_addr, uint32_t a_mass_erase ) {
  // заносим адрес
  EEPROM_REGS->EEA = a_in_eeprom_addr;
  // перед заполнением буфера страницы
  EEPROM_REGS->EECON = EEPROM_EECON_BWE_M
                     | EEPROM_EECON_WRBEH(EEPROM_EECON_WRBEH_ONE_PAGE)
                     ;
  // тут пишут, всё равно что заливать
  for ( uint32_t i = 0; i < (EEPROM_PAGE_SIZE / sizeof(uint32_t)); ++i ) {
    EEPROM_REGS->EEDAT = 0;
  }
  // запуск стирания
  EEPROM_REGS->EECON = EEPROM_EECON_BWE_M
                     | (0 == a_mass_erase ? EEPROM_EECON_WRBEH(EEPROM_EECON_WRBEH_ONE_PAGE) : EEPROM_EECON_WRBEH(EEPROM_EECON_WRBEH_ALL_PAGES))
                     | EEPROM_EECON_OP(EEPROM_EECON_OP_ERASE)
                     | EEPROM_EECON_EX_M
                     ;
  // ждём
  while ( 0 != (EEPROM_REGS->EESTA & EEPROM_EESTA_BSY_M) ) {}
}
