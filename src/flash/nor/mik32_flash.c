/**
 *
 *  MIK32 Amur external SPIFI flash memory driver
 * 
 */
 
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "imp.h"
#include <helper/binarybuffer.h>
#include <target/algorithm.h>
#include <target/armv7m.h>

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>



#define FLASH_SECTOR_SIZE   0x00001000 // 4096
#define FLASH_PAGE_SIZE     256
#define FLASH_ADDR_BASE     0x80000000
#define FLASH_MAIN_SIZE     0x00800000 // 8388608


#define STATUS_REGISTER_QE (1<<1)


#define FLASH_DRIVER_VER    0x00000009

/**
 * Private data for flash driver.
 */
typedef struct {
    /* target params */
    bool probed;
    char chip_name[64];
  // размер флэша в байтах
  uint32_t fsize;
  // буфер под сектор флэша
    uint8_t fsector[FLASH_SECTOR_SIZE];
  // результат выполнения команды Read JEDEC ID
  uint32_t jedec_id;
} mik32_flash_bank_t;


////////////////////////////////////////////////////////////////////////////////////////////////

static const uint8_t mik32_init_spifi_write_code[] = {
  #include "../../../contrib/loaders/flash/mikron/mik32_init_spifi_write.inc"
};


static int mik32_spifi_init_write( struct flash_bank *bank ) {
  int retval;
    struct target *target = bank->target;
    struct working_area *write_algorithm;
  struct reg_param reg_params[2];
  
  // место в ОЗУ контроллера для размещения кода инициализации SPI-FLASH на запись
  // + 256 байтов для стека
  // + 4 байта для результата
  // + 4 байта для выравнивания
  retval = target_alloc_working_area(
                  target
                , sizeof(mik32_init_spifi_write_code) + (256u + 4u + 4u)
                , &write_algorithm
                );
    if ( ERROR_OK != retval ) {
        LOG_WARNING( "no working area available, can't do block memory writes" );
        return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
    }

  // заносим код для исполнения в ОЗУ контроллера
    retval = target_write_buffer(
                target
              , write_algorithm->address
              , sizeof(mik32_init_spifi_write_code)
              , mik32_init_spifi_write_code
              );
    if ( ERROR_OK != retval ){
        target_free_working_area( target, write_algorithm );
        return retval;
    }

  // входной параметр - адрес для сохранения результата
    init_reg_param( &reg_params[0], "a0", 32, PARAM_OUT );
    init_reg_param( &reg_params[1], "sp", 32, PARAM_OUT );
  
  uint32_t v_result_addr = write_algorithm->address
                         + (sizeof(mik32_init_spifi_write_code) + 256u + 4u + 4u);
  // выравниваем на 4
  v_result_addr &= ~0x03;
  v_result_addr -= 4u;
  // подставляем фактические значения
  // адрес результата
  buf_set_u32( reg_params[0].value, 0, 32, v_result_addr );
  // адрес вершины стека
  buf_set_u32( reg_params[1].value, 0, 32, v_result_addr );
  // запускаем на исполнение
  retval = target_run_algorithm(
                target
              , 0
              , NULL
              , 2
              , reg_params
              , write_algorithm->address
              , write_algorithm->address + 2
              , 100
              , NULL
              );
  if (retval != ERROR_OK) {
    LOG_ERROR( "Failed to execute algorithm at target address 0x%x", (unsigned int)write_algorithm->address ); 
    target_free_working_area( target, write_algorithm );
    destroy_reg_param(&reg_params[0]);
    destroy_reg_param(&reg_params[1]);
    return retval;
  }
  
    mik32_flash_bank_t * bank_info = bank->driver_priv;
  
  retval = target_read_u32( target, v_result_addr, &bank_info->jedec_id );
  
  target_free_working_area( target, write_algorithm );
    destroy_reg_param(&reg_params[0]);
    destroy_reg_param(&reg_params[1]);
  return retval;
}


static int mik32_spifi_init_write2( struct target *target ) {
  int retval;
    struct working_area *write_algorithm;
  struct reg_param reg_params[2];

  // место в ОЗУ контроллера для размещения кода инициализации SPI-FLASH на запись
  // + 256 байтов для стека
  // + 4 байта для результата
  // + 4 байта для выравнивания
  retval = target_alloc_working_area(
                  target
                , sizeof(mik32_init_spifi_write_code) + (256u + 4u + 4u)
                , &write_algorithm
                );
    if ( ERROR_OK != retval ) {
        LOG_WARNING( "no working area available, can't do block memory writes" );
        return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
    }

  // заносим код для исполнения в ОЗУ контроллера
    retval = target_write_buffer(
                target
              , write_algorithm->address
              , sizeof(mik32_init_spifi_write_code)
              , mik32_init_spifi_write_code
              );
    if ( ERROR_OK != retval ){
        target_free_working_area( target, write_algorithm );
        return retval;
    }

  // входной параметр - адрес для сохранения результата
    init_reg_param( &reg_params[0], "a0", 32, PARAM_OUT );
    init_reg_param( &reg_params[1], "sp", 32, PARAM_OUT );
  
  uint32_t v_result_addr = write_algorithm->address
                         + (sizeof(mik32_init_spifi_write_code) + 256u + 4u + 4u);
  // выравниваем на 4
  v_result_addr &= ~0x03;
  v_result_addr -= 4u;
  // подставляем фактические значения
  // адрес результата
  buf_set_u32( reg_params[0].value, 0, 32, v_result_addr );
  // адрес вершины стека
  buf_set_u32( reg_params[1].value, 0, 32, v_result_addr );
  // запускаем на исполнение
  retval = target_run_algorithm(
                target
              , 0
              , NULL
              , 2
              , reg_params
              , write_algorithm->address
              , write_algorithm->address + 2
              , 100
              , NULL
              );
  if (retval != ERROR_OK) {
    LOG_ERROR( "Failed to execute algorithm at target address 0x%x", (unsigned int)write_algorithm->address ); 
    target_free_working_area( target, write_algorithm );
    destroy_reg_param(&reg_params[0]);
    destroy_reg_param(&reg_params[1]);
    return retval;
  }
  
  target_free_working_area( target, write_algorithm );
    destroy_reg_param(&reg_params[0]);
    destroy_reg_param(&reg_params[1]);
  return retval;
}



static const uint8_t mik32_init_spifi_read_code[] = {
  #include "../../../contrib/loaders/flash/mikron/mik32_init_spifi_read.inc"
};

// настройка контроллера SPIFI на режим XIP
static int mik32_spifi_init_read( struct target *target ) {
  int retval;
    struct working_area *read_algorithm;
  struct reg_param reg_params;

  // место в ОЗУ контроллера для размещения кода инициализации SPI-FLASH на запись
  // + 256 байтов для стека
  // + 4 байта для выравнивания
  retval = target_alloc_working_area(
                  target
                , sizeof(mik32_init_spifi_read_code) + (256u + 4u)
                , &read_algorithm
                );
    if ( ERROR_OK != retval ) {
        LOG_WARNING( "no working area available, can't do block memory writes" );
        return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
    }

  // заносим код для исполнения в ОЗУ контроллера
    retval = target_write_buffer(
                target
              , read_algorithm->address
              , sizeof(mik32_init_spifi_read_code)
              , mik32_init_spifi_read_code
              );
    if ( ERROR_OK != retval ){
        target_free_working_area( target, read_algorithm );
        return retval;
    }

    init_reg_param( &reg_params, "sp", 32, PARAM_OUT );
  
  uint32_t v_stack_addr = read_algorithm->address
                         + (sizeof(mik32_init_spifi_read_code) + 256u + 4u);
  // выравниваем на 4
  v_stack_addr &= ~0x03;
  // подставляем фактическое значение для указателя стека
  buf_set_u32( reg_params.value, 0, 32, v_stack_addr );
  // запускаем на исполнение
  retval = target_run_algorithm(
                target
              , 0
              , NULL
              , 1
              , &reg_params
              , read_algorithm->address
              , read_algorithm->address + 2
              , 100
              , NULL
              );
  if (retval != ERROR_OK) {
    LOG_ERROR( "Failed to execute algorithm at target address 0x%x", (unsigned int)read_algorithm->address ); 
  }
  
  target_free_working_area( target, read_algorithm );
    destroy_reg_param(&reg_params);
  return retval;
}


static const uint8_t mik32_spifi_chip_erase_code[] = {
  #include "../../../contrib/loaders/flash/mikron/mik32_spifi_chip_erase.inc"
};

static int mik32_mass_erase( struct target *target ) {
  int retval;
    struct working_area *work_algorithm;
  struct reg_param reg_params;

  // место в ОЗУ контроллера для размещения кода инициализации SPI-FLASH на запись
  // + 256 байтов для стека
  // + 4 байта для выравнивания
  retval = target_alloc_working_area(
                  target
                , sizeof(mik32_spifi_chip_erase_code) + (256u + 4u)
                , &work_algorithm
                );
    if ( ERROR_OK != retval ) {
        LOG_WARNING( "no working area available, can't do block memory writes" );
        return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
    }

  // заносим код для исполнения в ОЗУ контроллера
    retval = target_write_buffer(
                target
              , work_algorithm->address
              , sizeof(mik32_spifi_chip_erase_code)
              , mik32_spifi_chip_erase_code
              );
    if ( ERROR_OK != retval ){
        target_free_working_area( target, work_algorithm );
        return retval;
    }

    init_reg_param( &reg_params, "sp", 32, PARAM_OUT );
  
  uint32_t v_stack_addr = work_algorithm->address
                         + (sizeof(mik32_spifi_chip_erase_code) + 256u + 4u);
  // выравниваем на 4
  v_stack_addr &= ~0x03;
  // подставляем фактическое значение для указателя стека
  buf_set_u32( reg_params.value, 0, 32, v_stack_addr );
  // запускаем на исполнение
  retval = target_run_algorithm(
                target
              , 0
              , NULL
              , 1
              , &reg_params
              , work_algorithm->address
              , work_algorithm->address + 2
              , 120000
              , NULL
              );
  if (retval != ERROR_OK) {
    LOG_ERROR( "Failed to execute algorithm at target address 0x%x", (unsigned int)work_algorithm->address ); 
  }
  
  target_free_working_area( target, work_algorithm );
    destroy_reg_param(&reg_params);
  return retval;
}


static const uint8_t mik32_spifi_sector_erase_code[] = {
  #include "../../../contrib/loaders/flash/mikron/mik32_spifi_sector_erase.inc"
};

static int mik32_sector_erase(struct target *target, int sector_num) {
  int retval;
    struct working_area *write_algorithm;
  struct reg_param reg_params[2];

  // место в ОЗУ контроллера для размещения кода сектора
  // + 256 байтов для стека
  // + 4 байта для выравнивания
  retval = target_alloc_working_area(
                  target
                , sizeof(mik32_spifi_sector_erase_code) + (256u + 4u)
                , &write_algorithm
                );
    if ( ERROR_OK != retval ) {
        LOG_WARNING( "no working area available, can't do block memory writes" );
        return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
    }

  // заносим код для исполнения в ОЗУ контроллера
    retval = target_write_buffer(
                target
              , write_algorithm->address
              , sizeof(mik32_spifi_sector_erase_code)
              , mik32_spifi_sector_erase_code
              );
    if ( ERROR_OK != retval ){
        target_free_working_area( target, write_algorithm );
        return retval;
    }

  // входной параметр - адрес сектора
    init_reg_param( &reg_params[0], "a0", 32, PARAM_OUT );
  // стек
    init_reg_param( &reg_params[1], "sp", 32, PARAM_OUT );
  
  uint32_t v_end_addr = write_algorithm->address
                         + (sizeof(mik32_spifi_sector_erase_code) + 256u + 4u);
  // выравниваем на 4
  v_end_addr &= ~0x03;
  // подставляем фактические значения
  // адрес сектора
  buf_set_u32( reg_params[0].value, 0, 32, sector_num * FLASH_SECTOR_SIZE );
  // адрес вершины стека
  buf_set_u32( reg_params[1].value, 0, 32, v_end_addr );
  // запускаем на исполнение
  retval = target_run_algorithm(
                target
              , 0
              , NULL
              , 2
              , reg_params
              , write_algorithm->address
              , write_algorithm->address + 2
              , 500
              , NULL
              );
  if (retval != ERROR_OK) {
    LOG_ERROR( "Failed to execute algorithm at target address 0x%x", (unsigned int)write_algorithm->address ); 
  }
  
  target_free_working_area( target, write_algorithm );
    destroy_reg_param(&reg_params[0]);
    destroy_reg_param(&reg_params[1]);
  return retval;
}


static int mik32_erase(struct flash_bank *bank, unsigned int first, unsigned int last)
{
    int retval = ERROR_FLASH_OPERATION_FAILED;
    struct target *target = bank->target;
  
    LOG_INFO("Erasing sectors from %u to %u", first, last);
    if ( target->state != TARGET_HALTED ) {
        LOG_ERROR("Target not halted");
        return ERROR_TARGET_NOT_HALTED;
    }

  retval = mik32_spifi_init_write( bank );
  if ( ERROR_OK != retval ) {
    return retval;
  }


    if ( (0 == first) && ((bank->num_sectors - 1) == last) ) {
    // chip erase
        retval = mik32_mass_erase( target );
        if ( ERROR_OK != retval ) {
            return retval;
    }
    for (unsigned int i = 0; i < bank->num_sectors; ++i) {
      bank->sectors[i].is_erased = 1;
    }
    } else {
        // erasing pages
        for ( unsigned int i = first; i <= last; ++i ) {
            retval = mik32_sector_erase( target, i );
            if ( ERROR_OK != retval ) {
                return retval;
      }
            bank->sectors[i].is_erased = 1;
        }
    }

    return mik32_spifi_init_read( target );
}


static const uint8_t mik32_spifi_write_sector_code[] = {
  #include "../../../contrib/loaders/flash/mikron/mik32_spifi_write_sector.inc"
};

static int mik32_write_block( struct flash_bank *bank, const uint8_t *buffer, uint32_t offset, uint32_t count ) {
  int retval;
    struct target *target = bank->target;
    struct working_area *write_algorithm;
  struct reg_param reg_params[3];

  // mik32_spifi_write_sector_code пишет один сектор флэша.
  // offset на входе - это собственно адрес в "пространстве" флэша
  // задача - нарезать входной блок данных на секторы и записать их
  // с учётом начального адреса
  mik32_flash_bank_t * bank_info = bank->driver_priv;

  // загружаем прогу
  // место в ОЗУ контроллера для размещения кода записи сектора
  // + 4К для сектора
  // + 256 байтов для стека
  // + 4 байта для выравнивания
  retval = target_alloc_working_area(
                  target
                , sizeof(mik32_spifi_write_sector_code) + (4096u + 256u + 4u)
                , &write_algorithm
                );
    if ( ERROR_OK != retval ) {
        LOG_WARNING( "no working area available, can't do block memory writes" );
        return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
    }

  // заносим код для исполнения в ОЗУ контроллера
    retval = target_write_buffer(
                target
              , write_algorithm->address
              , sizeof(mik32_spifi_write_sector_code)
              , mik32_spifi_write_sector_code
              );
    if ( ERROR_OK != retval ){
        target_free_working_area( target, write_algorithm );
        return retval;
    }

  // входной параметр - адрес сектора в пямяти
    init_reg_param( &reg_params[0], "a0", 32, PARAM_OUT );
  // адрес сектора во флэше
    init_reg_param( &reg_params[1], "a1", 32, PARAM_OUT );
  // стек
    init_reg_param( &reg_params[2], "sp", 32, PARAM_OUT );
  
  uint32_t v_end_addr = write_algorithm->address
                         + (sizeof(mik32_spifi_write_sector_code) + 4096u + 256u + 4u);
  // выравниваем на 4
  v_end_addr &= ~0x03;
  //
  do {
    //
    uint32_t v_in_sector_offset = offset & (4096u - 1u);
    uint32_t v_in_sector_count = 4096u - v_in_sector_offset;
    // заполнитель, если адрес не выровнен по границе сектора
    memset( bank_info->fsector, 0xFF, 4096u - v_in_sector_count );
    //
    if ( v_in_sector_count > count ) {
      v_in_sector_count = count;
    }
    // заполнитель, если пишем не полный сектор
    memset( &bank_info->fsector[v_in_sector_offset + v_in_sector_count], 0xFF, 4096u - (v_in_sector_offset + v_in_sector_count) );
    // собственно данные
    memcpy( &bank_info->fsector[v_in_sector_offset], buffer, v_in_sector_count );
    //
    retval = target_write_buffer( target, v_end_addr - (4096u + 256u), 4096u, bank_info->fsector );
    if ( ERROR_OK != retval ) {
        target_free_working_area( target, write_algorithm );
      destroy_reg_param(&reg_params[0]);
      destroy_reg_param(&reg_params[1]);
      destroy_reg_param(&reg_params[2]);
      return retval;
    }
    // подставляем фактические значения для неизменных параметров
    // адрес данных
    buf_set_u32( reg_params[0].value, 0, 32, v_end_addr - (4096u + 256u) );
    // адрес сектора во флэше
    buf_set_u32( reg_params[1].value, 0, 32, offset & ~(4096u - 1u) );
    // адрес вершины стека
    buf_set_u32( reg_params[2].value, 0, 32, v_end_addr );
    // запускаем на исполнение
    retval = target_run_algorithm(
                  target
                , 0
                , NULL
                , 3
                , reg_params
                , write_algorithm->address
                , write_algorithm->address + 2
                , 100
                , NULL
                );
    if ( ERROR_OK != retval ) {
        target_free_working_area( target, write_algorithm );
      destroy_reg_param(&reg_params[0]);
      destroy_reg_param(&reg_params[1]);
      destroy_reg_param(&reg_params[2]);
      return retval;
    }
    //
    offset += v_in_sector_count;
    count -= v_in_sector_count;
    buffer += v_in_sector_count;
  } while ( 0 != count );
  
  target_free_working_area( target, write_algorithm );
    destroy_reg_param(&reg_params[0]);
    destroy_reg_param(&reg_params[1]);
    destroy_reg_param(&reg_params[2]);
  return ERROR_OK;
}


static int mik32_write( struct flash_bank *bank, const uint8_t *buffer, uint32_t offset, uint32_t count ) {
  int retval;

    if (bank->target->state != TARGET_HALTED) {
        LOG_ERROR("Target not halted");
        return ERROR_TARGET_NOT_HALTED;
    }

  LOG_INFO( "Write data to SPI Flash" );
  
  retval = mik32_spifi_init_write( bank );
  if ( ERROR_OK != retval ) {
    return retval;
  }

    /* try using block write */
    retval = mik32_write_block( bank, buffer, offset, count );
  if ( ERROR_OK != retval ) {
    return retval;
  }
  
  return mik32_spifi_init_read( bank->target );
}


static int mik32_read( struct flash_bank *bank, uint8_t *buffer, uint32_t offset, uint32_t count ) {
  int retval;
    struct target *target = bank->target;

  LOG_INFO( "%s", __PRETTY_FUNCTION__ );

    if (target->state != TARGET_HALTED) {
        LOG_ERROR("Target not halted");
        return ERROR_TARGET_NOT_HALTED;
    }
  
  retval = mik32_spifi_init_read( target );
  if ( ERROR_OK != retval ) {
    return retval;
  }

  return target_read_buffer( target, bank->base + offset, count, buffer );
}


static int mik32_probe( struct flash_bank * bank ) {
  int retval;
    struct target *target = bank->target;

    if (target->state != TARGET_HALTED) {
        LOG_ERROR( "Target not halted" );
        return ERROR_TARGET_NOT_HALTED;
    }

    mik32_flash_bank_t * bank_info = bank->driver_priv;

  retval = mik32_spifi_init_write( bank );
  if ( ERROR_OK == retval ) {
    int v_cid = (bank_info->jedec_id >> 16) & 0xFF;
    int v_size_kib = 0;
    if ( v_cid >= 10 ) {
      // int для v_size_kib даёт запас до 4 TiB ёмкости :)
      v_size_kib = (int)(1 << (v_cid - 10));
    }
    LOG_INFO( "Detected SPI flash" );
    LOG_INFO( "  Manufacturer ID: %02Xh", bank_info->jedec_id & 0xFF );
    LOG_INFO( "      Memory type: %02Xh", (bank_info->jedec_id >> 8) & 0xFF );
    LOG_INFO( "         Capacity: %uKiB (%02Xh)", v_size_kib, v_cid );
    retval = mik32_spifi_init_read( target );
    if ( ERROR_OK == retval ) {
      bank->size = v_size_kib * 1024;
      bank->num_sectors = bank->size / FLASH_SECTOR_SIZE;
      bank->sectors = malloc(sizeof(struct flash_sector) * bank->num_sectors);

      for (unsigned int i = 0; i < bank->num_sectors; ++i) {
        bank->sectors[i].offset = i * FLASH_SECTOR_SIZE;
        bank->sectors[i].size = FLASH_SECTOR_SIZE;
        bank->sectors[i].is_erased = -1;
        bank->sectors[i].is_protected = 0;
      }
      bank_info->probed = true;
    }
  } else {
    LOG_ERROR( "Unknown SPI flash type" );
  }
  
  return retval;
}


static int mik32_auto_probe( struct flash_bank *bank ) {
    mik32_flash_bank_t * bank_info = bank->driver_priv;
    if ( bank_info->probed ) {
        return ERROR_OK;
  } else {
    return mik32_probe( bank );
  }
}


static int mik32_erase_check( struct flash_bank *bank ) {
  LOG_INFO( "%s", __PRETTY_FUNCTION__ );
  return ERROR_OK;
}

static int mik32_verify(struct flash_bank *bank, const uint8_t *buffer, uint32_t offset, uint32_t count) {
  LOG_INFO( "%s", __PRETTY_FUNCTION__ );
  return ERROR_OK;
}

static int mik32_info( struct flash_bank *bank, struct command_invocation *cmd )
{
    command_print_sameline(
      cmd
    , "\nMikron \nMIK32 Amur\n----\n"
    );

    return ERROR_OK;
}


COMMAND_HANDLER(mik32_handle_driver_info_command)
{
    command_print(CMD, "mik32 Amur spi-flash driver\n"
                           "version: %d.%d\n",
                           FLASH_DRIVER_VER>>16,
                           FLASH_DRIVER_VER&0xFFFF);

    return ERROR_OK;
}


COMMAND_HANDLER(mik32_handle_mass_erase_command)
{
    int retval;
    struct target *target = get_current_target(CMD_CTX);

    if (target->state != TARGET_HALTED) {
        LOG_ERROR("Target not halted");
        return ERROR_TARGET_NOT_HALTED;
    }

  retval = mik32_spifi_init_write2( target );
  if ( ERROR_OK != retval ) {
    command_print(CMD, "Error init write");
    return retval;
  }

    command_print(CMD, "Chip erase\nPlease wait ... \n");

    retval = mik32_mass_erase( target );
    if (retval != ERROR_OK) {
        return retval;
  }
  
  retval = mik32_spifi_init_read( target );
    if (retval != ERROR_OK) {
        return retval;
  }

    command_print(CMD, "done!\n");

    return retval;
}


COMMAND_HANDLER(mik32_handle_erase_command)
{
    if (CMD_ARGC < 2)
        return ERROR_COMMAND_SYNTAX_ERROR;

    int retval;
    struct target *target = get_current_target(CMD_CTX);

    if (target->state != TARGET_HALTED) {
        LOG_ERROR("Target not halted");
        return ERROR_TARGET_NOT_HALTED;
    }

  retval = mik32_spifi_init_write2( target );
  if ( ERROR_OK != retval ) {
    command_print(CMD, "Error init write");
    return retval;
  }


    unsigned int first, last;
    COMMAND_PARSE_NUMBER(uint, CMD_ARGV[0], first);
    COMMAND_PARSE_NUMBER(uint, CMD_ARGV[1], last);

    command_print(CMD, "Erase flash sectors %d through %d\nPlease wait ... \n", first, last);

    for (unsigned int i = first; i <= last; i++) {
        retval = mik32_sector_erase( target, i );
        if (retval != ERROR_OK)
            return retval;
    }

  retval = mik32_spifi_init_read( target );
    if (retval != ERROR_OK) {
        return retval;
  }

    command_print(CMD, "done!\n");

    return retval;
}


static const struct command_registration mik32_exec_command_handlers[] = {
    {
        .name = "driver_info",
        .handler = mik32_handle_driver_info_command,
        .mode = COMMAND_EXEC,
        .usage = "",
        .help = "Show information about flash driver",
    },
    {
        .name = "mass_erase",
        .handler = mik32_handle_mass_erase_command,
        .mode = COMMAND_EXEC,
        .usage = "",
        .help = "Erase entire flash",
    },
    {
        .name = "erase",
        .handler = mik32_handle_erase_command,
        .mode = COMMAND_EXEC,
        .usage = "first_sector_num last_sector_num",
        .help = "Erase sectors of flash, starting at sector first up to and including last",
    },
    COMMAND_REGISTRATION_DONE
};

static const struct command_registration mik32_command_handlers[] = {
    {
        .name = "mik32_flash",
        .mode = COMMAND_ANY,
        .help = "mik32_flash command group",
        .usage = "",
        .chain = mik32_exec_command_handlers,
    },
    COMMAND_REGISTRATION_DONE
};


FLASH_BANK_COMMAND_HANDLER(mik32_flash_bank_command)
{
    mik32_flash_bank_t * bank_info;

    if (CMD_ARGC < 6) {
        return ERROR_COMMAND_SYNTAX_ERROR;
  }

    bank_info = malloc(sizeof(*bank_info));

    bank->driver_priv = bank_info;

    /* information will be updated by probing */
    bank_info->probed = false;
  snprintf( bank_info->chip_name, sizeof(bank_info->chip_name), "%s", CMD_ARGV[0] );
  bank_info->fsize = strtoul( CMD_ARGV[3], 0, 10 );
  if ( 0 == bank_info->fsize ) {
    LOG_ERROR( "Flash size not specified, using default value 8MiB" );
    bank_info->fsize = (8u * 1024u *1024u);
  }
  if ( 0 != (bank_info->fsize % FLASH_SECTOR_SIZE) ) {
    LOG_ERROR( "Wrong flash size specified, using default value 8MiB" );
    bank_info->fsize = (8u * 1024u *1024u);
  }

    return ERROR_OK;
}



const struct flash_driver mik32_flash = {
    .name = "mik32_flash",
    .usage = "flash bank <name> mik32_flash <base> <size> 0 0 <target#>",
    .commands = mik32_command_handlers,
    .flash_bank_command = mik32_flash_bank_command,
    .erase = mik32_erase,
    .protect = NULL,
    .write = mik32_write,
    .read = mik32_read,
    .probe = mik32_probe,
    .auto_probe = mik32_auto_probe,
    .erase_check = mik32_erase_check,
    .verify = mik32_verify,
    .protect_check = NULL,
    .info = mik32_info,
    .free_driver_priv = default_flash_free_driver_priv,
};


