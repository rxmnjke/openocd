/***************************************************************************
 *   Copyright (C) 2005 by Dominic Rath                                    *
 *   Dominic.Rath@gmx.de                                                   *
 *                                                                         *
 *   Copyright (C) 2008 by Spencer Oliver                                  *
 *   spen@spen-soft.co.uk                                                  *
 *                                                                         *
 *   Copyright (C) 2011 by Andreas Fritiofson                              *
 *   andreas.fritiofson@gmail.com                                          *
 *                                                                         *
 *   Copyright (C) 2013 by Paul Fertser                                    *
 *   fercerpav@gmail.com                                                   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "imp.h"
#include <helper/binarybuffer.h>
#include <target/algorithm.h>
#include <target/riscv/riscv.h>

//#undef  LOG_DEBUG
//#define LOG_DEBUG LOG_INFO

#define MD_RST_CLK            0x50020000
#define MD_PER2_CLOCK         (MD_RST_CLK + 0x1C)
#define MD_CPU_CLOCK          (MD_RST_CLK + 0x0C)
#define MD_PER2_CLOCK_FLASH  (1 << 3)
#define MD_PER2_CLOCK_RST_CLK (1 << 4)

#define FLASH_REG_BASE  0x50018000
#define FLASH_CMD       (FLASH_REG_BASE + 0x00)
#define FLASH_ADR       (FLASH_REG_BASE + 0x04)
#define FLASH_DI        (FLASH_REG_BASE + 0x08)
#define FLASH_DO        (FLASH_REG_BASE + 0x0C)
#define FLASH_KEY       (FLASH_REG_BASE + 0x10)
#define FLASH_CTRL      (FLASH_REG_BASE + 0x14)

#define FLASH_TMR   (1 << 14)
#define FLASH_NVSTR (1 << 13)
#define FLASH_PROG  (1 << 12)
#define FLASH_MAS1  (1 << 11)
#define FLASH_ERASE (1 << 10)
#define FLASH_IFREN (1 << 9)
#define FLASH_SE    (1 << 8)
#define FLASH_YE    (1 << 7)
#define FLASH_XE    (1 << 6)
#define FLASH_RD    (1 << 2)
#define FLASH_WR    (1 << 1)
#define FLASH_CON   (1 << 0)
#define FLASH_DELAY_MASK    (1 << 3)

#define FLASH_Tnvs   10    // 5..
#define FLASH_Terase 30000 // 20000..40000
#define FLASH_Tme    30000 // 20000..40000
#define FLASH_Tnvh   10    // 5..
#define FLASH_Tnvh1  200   // 100..
#define FLASH_Tprog  30    // 20..40
#define FLASH_Tpgs   20    // 10..
#define FLASH_Trcv   20    // 10..

#define KEY     0x8AAA5551

struct mdr217_flash_bank {
    int probed;
    unsigned int mem_type;
    unsigned int page_count;
    unsigned int sec_count;
};
    /* see ... for src */
static const uint8_t mdr217_flash_write_code[] = {
0x37, 0x87, 0x01, 0x50, 0xB7, 0x57, 0xAA, 0x8A, 0x93, 0x87, 0x17, 0x55,
0x23, 0x28, 0xF7, 0x00, 0x83, 0x27, 0x07, 0x00, 0x93, 0xF7, 0x87, 0x03,
0x37, 0x48, 0x00, 0x00, 0x13, 0x08, 0x18, 0x00, 0x33, 0xE8, 0x07, 0x01,
0x63, 0x08, 0x05, 0x00, 0x37, 0x48, 0x00, 0x00, 0x13, 0x08, 0x18, 0x20,
0x33, 0xE8, 0x07, 0x01, 0xB7, 0x87, 0x01, 0x50, 0x23, 0xA0, 0x07, 0x01,
0x6F, 0x00, 0x00, 0x01, 0x13, 0x06, 0x46, 0x00, 0x93, 0x86, 0x46, 0x00,
0x93, 0x85, 0xC5, 0xFF, 0x63, 0x8E, 0x05, 0x0E, 0x37, 0x87, 0x01, 0x50,
0x23, 0x22, 0xD7, 0x00, 0x83, 0x27, 0x06, 0x00, 0x23, 0x24, 0xF7, 0x00,
0xB7, 0x17, 0x00, 0x00, 0x93, 0x87, 0x07, 0x04, 0xB3, 0x67, 0xF8, 0x00,
0x23, 0x20, 0xF7, 0x00, 0x73, 0x27, 0x00, 0xB0, 0x13, 0x07, 0x07, 0x05,
0xF3, 0x27, 0x00, 0xB0, 0xB3, 0x87, 0xE7, 0x40, 0xE3, 0xCC, 0x07, 0xFE,
0xB7, 0x37, 0x00, 0x00, 0x93, 0x87, 0x07, 0x04, 0xB3, 0x67, 0xF8, 0x00,
0x37, 0x87, 0x01, 0x50, 0x23, 0x20, 0xF7, 0x00, 0x73, 0x27, 0x00, 0xB0,
0x13, 0x07, 0x07, 0x0A, 0xF3, 0x27, 0x00, 0xB0, 0xB3, 0x87, 0xE7, 0x40,
0xE3, 0xCC, 0x07, 0xFE, 0xB7, 0x37, 0x00, 0x00, 0x93, 0x87, 0x07, 0x0C,
0xB3, 0x67, 0xF8, 0x00, 0x37, 0x87, 0x01, 0x50, 0x23, 0x20, 0xF7, 0x00,
0x73, 0x27, 0x00, 0xB0, 0x13, 0x07, 0x07, 0x0F, 0xF3, 0x27, 0x00, 0xB0,
0xB3, 0x87, 0xE7, 0x40, 0xE3, 0xCC, 0x07, 0xFE, 0x93, 0x77, 0xF8, 0xF7,
0x37, 0x37, 0x00, 0x00, 0x13, 0x07, 0x07, 0x04, 0xB3, 0xE7, 0xE7, 0x00,
0x37, 0x85, 0x01, 0x50, 0x23, 0x20, 0xF5, 0x00, 0xB7, 0xF7, 0xFF, 0xFF,
0x93, 0x87, 0xF7, 0xF7, 0xB3, 0x77, 0xF8, 0x00, 0x37, 0x27, 0x00, 0x00,
0x13, 0x07, 0x07, 0x04, 0xB3, 0xE7, 0xE7, 0x00, 0x23, 0x20, 0xF5, 0x00,
0x73, 0x27, 0x00, 0xB0, 0x13, 0x07, 0x07, 0x05, 0xF3, 0x27, 0x00, 0xB0,
0xB3, 0x87, 0xE7, 0x40, 0xE3, 0xCC, 0x07, 0xFE, 0xB7, 0xD7, 0xFF, 0xFF,
0x93, 0x87, 0xF7, 0xF3, 0x33, 0x78, 0xF8, 0x00, 0xB7, 0x87, 0x01, 0x50,
0x23, 0xA0, 0x07, 0x01, 0x73, 0x27, 0x00, 0xB0, 0x13, 0x07, 0x07, 0x0A,
0xF3, 0x27, 0x00, 0xB0, 0xB3, 0x87, 0xE7, 0x40, 0xE3, 0xCC, 0x07, 0xFE,
0x6F, 0xF0, 0xDF, 0xEF, 0x93, 0x77, 0x88, 0x03, 0x37, 0x47, 0x00, 0x00,
0xB3, 0xE7, 0xE7, 0x00, 0x37, 0x87, 0x01, 0x50, 0x23, 0x20, 0xF7, 0x00,
0x23, 0x28, 0x07, 0x00, 0x93, 0x07, 0xA0, 0x02, 0x13, 0x85, 0x07, 0x00,
0x93, 0x85, 0x05, 0x00, 0x93, 0x86, 0x06, 0x00, 0x73, 0x00, 0x10, 0x00
};

static const unsigned char mdr217_flash_mass_erase[] =
{
0xB7, 0x87, 0x01, 0x50, 0x37, 0x57, 0xAA, 0x8A, 0x13, 0x07, 0x17, 0x55,
0x23, 0xA8, 0xE7, 0x00, 0x83, 0xA6, 0x07, 0x00, 0x93, 0xF6, 0x86, 0x03,
0x37, 0x47, 0x00, 0x00, 0x13, 0x07, 0x17, 0x00, 0xB3, 0xE6, 0xE6, 0x00,
0x23, 0xA0, 0xD7, 0x00, 0x13, 0x06, 0x00, 0x00, 0x6F, 0x00, 0xC0, 0x0B,
0x93, 0x17, 0x26, 0x01, 0x37, 0x87, 0x01, 0x50, 0x23, 0x22, 0xF7, 0x00,
0xB7, 0x17, 0x00, 0x00, 0x93, 0x87, 0x07, 0xC4, 0xB3, 0xE7, 0xF6, 0x00,
0x23, 0x20, 0xF7, 0x00, 0x73, 0x27, 0x00, 0xB0, 0x13, 0x07, 0x07, 0x05,
0xF3, 0x27, 0x00, 0xB0, 0xB3, 0x87, 0xE7, 0x40, 0xE3, 0xCC, 0x07, 0xFE,
0xB7, 0x37, 0x00, 0x00, 0x93, 0x87, 0x07, 0xC4, 0xB3, 0xE7, 0xF6, 0x00,
0x37, 0x87, 0x01, 0x50, 0x23, 0x20, 0xF7, 0x00, 0x73, 0x27, 0x00, 0xB0,
0xB7, 0xB7, 0x03, 0x00, 0x93, 0x87, 0x07, 0x98, 0x33, 0x07, 0xF7, 0x00,
0xF3, 0x27, 0x00, 0xB0, 0xB3, 0x87, 0xE7, 0x40, 0xE3, 0xCC, 0x07, 0xFE,
0x93, 0xF7, 0xF6, 0xBF, 0x37, 0x37, 0x00, 0x00, 0x13, 0x07, 0x07, 0x84,
0xB3, 0xE7, 0xE7, 0x00, 0x37, 0x87, 0x01, 0x50, 0x23, 0x20, 0xF7, 0x00,
0x73, 0x27, 0x00, 0xB0, 0x13, 0x07, 0x07, 0x64, 0xF3, 0x27, 0x00, 0xB0,
0xB3, 0x87, 0xE7, 0x40, 0xE3, 0xCC, 0x07, 0xFE, 0xB7, 0xD7, 0xFF, 0xFF,
0x93, 0x87, 0xF7, 0x3B, 0xB3, 0xF6, 0xF6, 0x00, 0xB7, 0x87, 0x01, 0x50,
0x23, 0xA0, 0xD7, 0x00, 0x73, 0x27, 0x00, 0xB0, 0x13, 0x07, 0x07, 0x0A,
0xF3, 0x27, 0x00, 0xB0, 0xB3, 0x87, 0xE7, 0x40, 0xE3, 0xCC, 0x07, 0xFE,
0x13, 0x06, 0x16, 0x00, 0x93, 0x07, 0x10, 0x00, 0xE3, 0xF2, 0xC7, 0xF4,
0x93, 0xF6, 0x86, 0x03, 0xB7, 0x47, 0x00, 0x00, 0xB3, 0xE6, 0xF6, 0x00,
0xB7, 0x87, 0x01, 0x50, 0x23, 0xA0, 0xD7, 0x00, 0x23, 0xA8, 0x07, 0x00,
0x93, 0x07, 0xE0, 0x0E, 0x13, 0x85, 0x07, 0x00, 0x73, 0x00, 0x10, 0x00
};

static const unsigned char mdr217_flash_erase_sect[] =
{
0x37, 0x87, 0x01, 0x50, 0xB7, 0x57, 0xAA, 0x8A, 0x93, 0x87, 0x17, 0x55,
0x23, 0x28, 0xF7, 0x00, 0x83, 0x27, 0x07, 0x00, 0x93, 0xF7, 0x87, 0x03,
0xB7, 0x46, 0x00, 0x00, 0x93, 0x86, 0x16, 0x00, 0xB3, 0xE6, 0xD7, 0x00,
0x63, 0x08, 0x05, 0x00, 0xB7, 0x46, 0x00, 0x00, 0x93, 0x86, 0x16, 0x20,
0xB3, 0xE6, 0xD7, 0x00, 0xB7, 0x87, 0x01, 0x50, 0x23, 0xA0, 0xD7, 0x00,
0x6F, 0x00, 0x40, 0x0B, 0x93, 0x97, 0xC5, 0x00, 0x37, 0x87, 0x01, 0x50,
0x23, 0x22, 0xF7, 0x00, 0x93, 0xE7, 0x06, 0x44, 0x23, 0x20, 0xF7, 0x00,
0x73, 0x27, 0x00, 0xB0, 0x13, 0x07, 0x07, 0x05, 0xF3, 0x27, 0x00, 0xB0,
0xB3, 0x87, 0xE7, 0x40, 0xE3, 0xCC, 0x07, 0xFE, 0xB7, 0x27, 0x00, 0x00,
0x93, 0x87, 0x07, 0x44, 0xB3, 0xE7, 0xF6, 0x00, 0x37, 0x87, 0x01, 0x50,
0x23, 0x20, 0xF7, 0x00, 0x73, 0x27, 0x00, 0xB0, 0xB7, 0xB7, 0x03, 0x00,
0x93, 0x87, 0x07, 0x98, 0x33, 0x07, 0xF7, 0x00, 0xF3, 0x27, 0x00, 0xB0,
0xB3, 0x87, 0xE7, 0x40, 0xE3, 0xCC, 0x07, 0xFE, 0x93, 0xF7, 0xF6, 0xBF,
0x37, 0x27, 0x00, 0x00, 0x13, 0x07, 0x07, 0x04, 0xB3, 0xE7, 0xE7, 0x00,
0x37, 0x87, 0x01, 0x50, 0x23, 0x20, 0xF7, 0x00, 0x73, 0x27, 0x00, 0xB0,
0x13, 0x07, 0x07, 0x05, 0xF3, 0x27, 0x00, 0xB0, 0xB3, 0x87, 0xE7, 0x40,
0xE3, 0xCC, 0x07, 0xFE, 0xB7, 0xE7, 0xFF, 0xFF, 0x93, 0x87, 0xF7, 0xBB,
0xB3, 0xF6, 0xF6, 0x00, 0xB7, 0x87, 0x01, 0x50, 0x23, 0xA0, 0xD7, 0x00,
0x73, 0x27, 0x00, 0xB0, 0x13, 0x07, 0x07, 0x0A, 0xF3, 0x27, 0x00, 0xB0,
0xB3, 0x87, 0xE7, 0x40, 0xE3, 0xCC, 0x07, 0xFE, 0x93, 0x85, 0x15, 0x00,
0xE3, 0x78, 0xB6, 0xF4, 0x93, 0xF7, 0x86, 0x03, 0x37, 0x47, 0x00, 0x00,
0xB3, 0xE7, 0xE7, 0x00, 0x37, 0x87, 0x01, 0x50, 0x23, 0x20, 0xF7, 0x00,
0x23, 0x28, 0x07, 0x00, 0x93, 0x07, 0xE0, 0x00, 0x13, 0x85, 0x07, 0x00,
0x73, 0x00, 0x10, 0x00
};

/* flash bank <name> mdr <base> <size> 0 0 <target#> <type> <page_count> <sec_count> */
FLASH_BANK_COMMAND_HANDLER(mdr217_flash_bank_command)
{
    struct mdr217_flash_bank *mdr217_info;

    if (CMD_ARGC < 9)
        return ERROR_COMMAND_SYNTAX_ERROR;

    mdr217_info = malloc(sizeof(struct mdr217_flash_bank));

    bank->driver_priv = mdr217_info;
    mdr217_info->probed = false;
    COMMAND_PARSE_NUMBER(uint, CMD_ARGV[6], mdr217_info->mem_type);
    COMMAND_PARSE_NUMBER(uint, CMD_ARGV[7], mdr217_info->page_count);
    COMMAND_PARSE_NUMBER(uint, CMD_ARGV[8], mdr217_info->sec_count);
    LOG_DEBUG("MDR217: flash bank");
    return ERROR_OK;
}

static int mdr217_flash_mass_erase_hw(struct flash_bank *bank)
{
    struct target *target = bank->target;
    struct working_area *hw_algorithm;
    struct reg_param reg_params[1];
    int retval = ERROR_OK;

//    LOG_INFO ("MDR217: ERASE CHIP HW @0x%"PRIX32"", (uint32_t)bank->base);
//
//  FILE *fp = NULL;
//  ssize_t r;
//  fp = fopen("mdr217_flash_mass_erase.bin", "wb");
//  r = fwrite(mdr217_flash_mass_erase, 1, sizeof(mdr217_flash_mass_erase), fp);
//  LOG_DEBUG("MDR217: file len %"PRId32" ", (unsigned int)r);
//  fclose(fp);

    /* flash erase code */
    LOG_DEBUG("MDR217: request %"PRId32" bytes of memory", (unsigned int)sizeof(mdr217_flash_mass_erase));
    if (sizeof(mdr217_flash_mass_erase)==0 || target_alloc_working_area(target, sizeof(mdr217_flash_mass_erase),
            &hw_algorithm) != ERROR_OK) {
        //LOG_WARNING("MDR217: no working area available, can't do hw memory erase");
        return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
    }

    retval = target_write_u32(target, MD_CPU_CLOCK, (1<<8) |(0<<4) |(0<<2) |(0<<0)); // CPU_C1=CPU_C2=CPU_C3=HCLK = HSI ~8MHz
    if (retval != ERROR_OK)
        goto free_buffer;

    retval = target_write_buffer(target, hw_algorithm->address,
            sizeof(mdr217_flash_mass_erase), mdr217_flash_mass_erase);
    if (retval != ERROR_OK)
       goto free_buffer;

    // a0=x10 s10=x26
    init_reg_param(&reg_params[0], "a0", 32, PARAM_IN); /* status (uC->PC) */

//int target_run_algorithm(struct target *target,
//      int num_mem_params, struct mem_param *mem_params,
//      int num_reg_params, struct reg_param *reg_param,
//      uint32_t entry_point, uint32_t exit_point,
//      int timeout_ms, void *arch_info)

    retval = target_run_algorithm(target,
            0, NULL,
            1, reg_params,
            hw_algorithm->address, 0,
            1000, NULL);

    LOG_DEBUG("MDR217: status      0x%"PRIX32"", buf_get_u32(reg_params[0].value, 0, 32));

    if (retval == ERROR_FLASH_OPERATION_FAILED) {
        LOG_ERROR("MDR217: flash erase failed");
        }
    else
        {
        for (uint32_t sect = 0; sect < bank->num_sectors; sect++) {
            bank->sectors[sect].is_erased = 1; //?
            }
        }

    destroy_reg_param(&reg_params[0]);

free_buffer:
    target_free_working_area(target, hw_algorithm);

    return retval;
}

static int mdr217_flash_erase_sect_hw(struct flash_bank *bank, unsigned int first, unsigned int last)
{
    struct target *target = bank->target;
    struct mdr217_flash_bank *mdr217_info = bank->driver_priv;
    struct working_area *hw_algorithm;
    struct reg_param reg_params[3];
    int retval = ERROR_OK;

    /* flash erase code */
    LOG_DEBUG("MDR217: request %"PRId32" bytes of memory", (unsigned int)sizeof(mdr217_flash_erase_sect));
    if (sizeof(mdr217_flash_erase_sect)==0 || target_alloc_working_area(target, sizeof(mdr217_flash_erase_sect),
            &hw_algorithm) != ERROR_OK) {
        //LOG_WARNING("MDR217: no working area available, can't do hw memory erase");
        return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
    }

    retval = target_write_u32(target, MD_CPU_CLOCK, (1<<8) |(0<<4) |(0<<2) |(0<<0)); // CPU_C1=CPU_C2=CPU_C3=HCLK = HSI ~8MHz
    if (retval != ERROR_OK)
        goto free_buffer;

    retval = target_write_buffer(target, hw_algorithm->address,
            sizeof(mdr217_flash_erase_sect), mdr217_flash_erase_sect);
    if (retval != ERROR_OK)
        goto free_buffer;

    // a0=x10 s10=x26
    init_reg_param(&reg_params[0], "a0", 32, PARAM_IN_OUT); /* mem_type (PC->uC), status (uC->PC) */
    init_reg_param(&reg_params[1], "a1", 32, PARAM_OUT);    /* first (PC->uC)*/
    init_reg_param(&reg_params[2], "a2", 32, PARAM_OUT);    /* last (PC->uC) */

    buf_set_u32(reg_params[0].value, 0, 32, mdr217_info->mem_type);
    buf_set_u32(reg_params[1].value, 0, 32, first);
    buf_set_u32(reg_params[2].value, 0, 32, last);

    LOG_DEBUG("MDR217: mem_type       0x%"PRIX32"", buf_get_u32(reg_params[0].value, 0, 32));
    LOG_DEBUG("MDR217: first          0x%"PRIX32"", buf_get_u32(reg_params[1].value, 0, 32));
    LOG_DEBUG("MDR217: last           0x%"PRIX32"", buf_get_u32(reg_params[2].value, 0, 32));

    //int target_run_algorithm(struct target *target,
    //      int num_mem_params, struct mem_param *mem_params,
    //      int num_reg_params, struct reg_param *reg_param,
    //      uint32_t entry_point, uint32_t exit_point,
    //      int timeout_ms, void *arch_info)

    retval = target_run_algorithm(target,
            0, NULL,
            3, reg_params,
            hw_algorithm->address, 0,
            1000+80*(last-first+1), NULL); // 40ms*2tol

    LOG_DEBUG("MDR217: status      0x%"PRIX32"", buf_get_u32(reg_params[0].value, 0, 32));

    if (retval == ERROR_FLASH_OPERATION_FAILED) {
        LOG_ERROR("MDR217: flash erase failed");
        }

    destroy_reg_param(&reg_params[0]);
    destroy_reg_param(&reg_params[1]);
    destroy_reg_param(&reg_params[2]);

free_buffer:
    target_free_working_area(target, hw_algorithm);

    return retval;
}

static int mdr217_mass_erase(struct flash_bank *bank) // MAIN memory only
{
    struct target *target = bank->target;
    struct mdr217_flash_bank *mdr217_info = bank->driver_priv;
    uint32_t flash_cmd;
    int retval, retval2;

    LOG_INFO ("MDR217: MASS ERASE %s", mdr217_info->mem_type?"INFO":"MAIN"); // , (uint32_t)bank->base

    /* try using hw erase */
    retval = mdr217_flash_mass_erase_hw(bank);

    if (retval == ERROR_TARGET_RESOURCE_NOT_AVAILABLE) {
        /* if hw erase failed (no sufficient working area),
         * we use normal (slow) single word accesses */
        LOG_WARNING("MDR217: Can't use hw erase, falling back to single memory accesses");

        retval = target_read_u32(target, FLASH_CMD, &flash_cmd);
        if (retval != ERROR_OK)
            return retval;

        /* Switch on register access */
        flash_cmd = (flash_cmd & FLASH_DELAY_MASK) | FLASH_CON | FLASH_TMR;
        retval = target_write_u32(target, FLASH_CMD, flash_cmd);
        if (retval != ERROR_OK)
            goto reset_pg_and_lock;

        for(uint32_t chip=0;chip<2;chip++) {
            uint32_t addr = (mdr217_info->mem_type? 0x00002000 : 0x00040000)*chip;
            LOG_DEBUG("MDR217: MASS ERASE ADR=0x%"PRIx32"", addr);
            retval = target_write_u32(target, FLASH_ADR, addr);
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;

            flash_cmd |= FLASH_XE | FLASH_MAS1 | FLASH_ERASE;
            retval = target_write_u32(target, FLASH_CMD, flash_cmd);
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;
            usleep(FLASH_Tnvs);
            flash_cmd |= FLASH_NVSTR;
            retval = target_write_u32(target, FLASH_CMD, flash_cmd);
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;
            usleep(FLASH_Tme);
            flash_cmd &= ~FLASH_ERASE;
            retval = target_write_u32(target, FLASH_CMD, flash_cmd);
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;
            usleep(FLASH_Tnvh1);
            flash_cmd &= ~FLASH_ERASE;
            retval = target_write_u32(target, FLASH_CMD, flash_cmd);
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;
            flash_cmd &= ~(FLASH_XE | FLASH_MAS1 | FLASH_NVSTR);
            retval = target_write_u32(target, FLASH_CMD, flash_cmd);
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;
            usleep(FLASH_Trcv);
        } // for chip
        for (uint32_t sect = 0; sect < bank->num_sectors; sect++) {
            bank->sectors[sect].is_erased = 1; //?
        } // for sect
    } // if (retval == ERROR_TARGET_RESOURCE_NOT_AVAILABLE)

reset_pg_and_lock:
    flash_cmd = (flash_cmd & FLASH_DELAY_MASK) | FLASH_TMR;
    retval2 = target_write_u32(target, FLASH_CMD, flash_cmd);
    if (retval == ERROR_OK)
        retval = retval2;

    retval2 = target_write_u32(target, FLASH_KEY, 0x0);
    if (retval == ERROR_OK)
        retval = retval2;

    return retval;
} // mdr217_mass_erase

static int mdr217_erase(struct flash_bank *bank, unsigned int first, unsigned int last)
{
    struct target *target = bank->target;
    struct mdr217_flash_bank *mdr217_info = bank->driver_priv;
    int retval, retval2;
    uint32_t flash_cmd, cur_per_clock;

    if (bank->target->state != TARGET_HALTED) {
        LOG_ERROR("MDR217: Target not halted");
        return ERROR_TARGET_NOT_HALTED;
    }

    retval = target_read_u32(target, MD_PER2_CLOCK, &cur_per_clock);
    if (retval != ERROR_OK)
        return retval;

    retval = target_write_u32(target, MD_PER2_CLOCK, cur_per_clock | MD_PER2_CLOCK_FLASH);
    if (retval != ERROR_OK)
        return retval;

    if (!(cur_per_clock & MD_PER2_CLOCK_RST_CLK)) {
        LOG_ERROR("MDR217: Target needs reset before flash operations");
        return ERROR_FLASH_OPERATION_FAILED;
    }

    retval = target_write_u32(target, FLASH_KEY, KEY);
    if (retval != ERROR_OK)
        return retval;

    if ((first == 0) && (last >= (bank->num_sectors - 1)) &&
        !mdr217_info->mem_type) {
        retval = mdr217_mass_erase(bank);
        goto reset_pg_and_lock;
    }

    LOG_INFO ("MDR217: ERASE %s sectors from %"PRId32" to %"PRId32"", mdr217_info->mem_type?"INFO":"MAIN", first, last); // , (uint32_t)bank->base

    /* try using hw erase */
    retval = mdr217_flash_erase_sect_hw(bank, first, last);

    if (retval == ERROR_TARGET_RESOURCE_NOT_AVAILABLE) {
        /* if hw erase failed (no sufficient working area),
         * we use normal (slow) single word accesses */
        LOG_WARNING("MDR217: Can't use hw erase, falling back to single memory accesses");

        retval = target_read_u32(target, FLASH_CMD, &flash_cmd);
        if (retval != ERROR_OK)
            goto reset_pg_and_lock;

        /* Switch on register access */
        flash_cmd = (flash_cmd & FLASH_DELAY_MASK) | FLASH_CON | FLASH_TMR;
        if (mdr217_info->mem_type)
            flash_cmd |= FLASH_IFREN;

        retval = target_write_u32(target, FLASH_CMD, flash_cmd);
        if (retval != ERROR_OK)
            goto reset_pg_and_lock;

        unsigned int page_size = bank->size / mdr217_info->page_count;
        unsigned int sect_size = bank->size / mdr217_info->sec_count;
        LOG_DEBUG("MDR217: page_size 0x%"PRIX32" page_count 0x%"PRIX32" sect_size 0x%"PRIX32" sec_count 0x%"PRIX32" num_sectors 0x%"PRIX32"", page_size, mdr217_info->page_count, sect_size, mdr217_info->sec_count, bank->num_sectors);

        for (uint32_t sect = first; sect <= last; sect++) {
            uint32_t addr = (sect * sect_size);
            LOG_DEBUG("MDR217: ERASE ADR=0x%"PRIx32"", addr);
            retval = target_write_u32(target, FLASH_ADR, addr);
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;

            flash_cmd |= FLASH_XE | FLASH_ERASE;
            retval = target_write_u32(target, FLASH_CMD, flash_cmd);
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;
            usleep(FLASH_Tnvs);
            flash_cmd |= FLASH_NVSTR;
            retval = target_write_u32(target, FLASH_CMD, flash_cmd);
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;
            usleep(FLASH_Terase);
            flash_cmd &= ~FLASH_ERASE;
            retval = target_write_u32(target, FLASH_CMD, flash_cmd);
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;
            usleep(FLASH_Tnvh);
            flash_cmd &= ~(FLASH_XE | FLASH_NVSTR);
            retval = target_write_u32(target, FLASH_CMD, flash_cmd);
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;
            usleep(FLASH_Trcv);
            bank->sectors[sect].is_erased = 1; //?
        } // for sect
    } // if (retval == ERROR_TARGET_RESOURCE_NOT_AVAILABLE)

reset_pg_and_lock:
    flash_cmd = (flash_cmd & FLASH_DELAY_MASK) | FLASH_TMR;
    retval2 = target_write_u32(target, FLASH_CMD, flash_cmd);
    if (retval == ERROR_OK)
        retval = retval2;

    retval2 = target_write_u32(target, FLASH_KEY, 0x0);
    if (retval == ERROR_OK)
        retval = retval2;

    return retval;
} // mdr217_erase

static int mdr217_write_hw(struct flash_bank *bank, const uint8_t *buffer,
        uint32_t offset, uint32_t count)
{
    struct target *target = bank->target;
    struct mdr217_flash_bank *mdr217_info = bank->driver_priv;
    unsigned int page_size = bank->size / mdr217_info->page_count;
    uint32_t buffer_size = page_size;
    struct working_area *hw_algorithm;
    struct working_area *source;
    uint32_t address = bank->base + offset;
    unsigned int bytes_to_write;
    struct reg_param reg_params[4];
    int retval = ERROR_OK;

    /* flash write code */
    LOG_DEBUG("MDR217: request %"PRId32" bytes of memory", (unsigned int)sizeof(mdr217_flash_write_code));
    if (sizeof(mdr217_flash_write_code)==0 || target_alloc_working_area(target, sizeof(mdr217_flash_write_code),
            &hw_algorithm) != ERROR_OK) {
        //LOG_WARNING("no working area available, can't do hw memory writes");
        return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
    }
    retval = target_write_u32(target, MD_CPU_CLOCK, (1<<8) |(0<<4) |(0<<2) |(0<<0)); // CPU_C1=CPU_C2=CPU_C3=HCLK = HSI ~8MHz
    if (retval != ERROR_OK)
        return retval;

    retval = target_write_buffer(target, hw_algorithm->address,
            sizeof(mdr217_flash_write_code), mdr217_flash_write_code);
    if (retval != ERROR_OK)
        return retval;

    bytes_to_write = buffer_size<count? buffer_size:count;// min(buffer_size, count);
    /* memory buffer */
    LOG_DEBUG("MDR217: request %"PRId32" bytes of memory", bytes_to_write);
    while (target_alloc_working_area_try(target, bytes_to_write, &source) != ERROR_OK) {
            /* we already allocated the writing code, but failed to get a
             * buffer, free the algorithm */
            target_free_working_area(target, hw_algorithm);

            //LOG_WARNING("MDR217: no large enough working area available, can't do hw memory writes");
            return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
//      }
    }
    while (count>0)    {
        bytes_to_write = buffer_size<count? buffer_size:count;// min(buffer_size, count);
    // a0=x10 s10=x26
        init_reg_param(&reg_params[0], "a0", 32, PARAM_IN_OUT); /* mem_type (PC->uC), status (uC->PC) */
        init_reg_param(&reg_params[1], "a1", 32, PARAM_IN_OUT); /* byte_count (PC->uC), byte_count (uC->PC) */
        init_reg_param(&reg_params[2], "a2", 32, PARAM_OUT);    /* buffer start (PC->uC)*/
        init_reg_param(&reg_params[3], "a3", 32, PARAM_IN_OUT); /* target address (PC->uC), target address (uC->PC)*/

        retval = target_write_buffer(target, source->address, bytes_to_write, buffer);
        if (retval != ERROR_OK)
            goto free_buffer;

        buf_set_u32(reg_params[0].value, 0, 32, mdr217_info->mem_type);
        buf_set_u32(reg_params[1].value, 0, 32, bytes_to_write);
        buf_set_u32(reg_params[2].value, 0, 32, source->address);
        buf_set_u32(reg_params[3].value, 0, 32, address);

        LOG_DEBUG("MDR217: mem_type       0x%"PRIX32"", buf_get_u32(reg_params[0].value, 0, 32));
        LOG_DEBUG("MDR217: byte_count     0x%"PRIX32"", buf_get_u32(reg_params[1].value, 0, 32));
        LOG_DEBUG("MDR217: start          0x%"PRIX32"", buf_get_u32(reg_params[2].value, 0, 32));
        LOG_DEBUG("MDR217: address        0x%"PRIX32"", buf_get_u32(reg_params[3].value, 0, 32));

    //int target_run_algorithm(struct target *target,
    //      int num_mem_params, struct mem_param *mem_params,
    //      int num_reg_params, struct reg_param *reg_param,
    //      uint32_t entry_point, uint32_t exit_point,
    //      int timeout_ms, void *arch_info)

        retval = target_run_algorithm(target,
                0, NULL,
                4, reg_params,
                hw_algorithm->address, 0,
                1000, NULL);

        LOG_DEBUG("MDR217: status      0x%"PRIX32"", buf_get_u32(reg_params[0].value, 0, 32));
        LOG_DEBUG("MDR217: byte_count  0x%"PRIX32"", buf_get_u32(reg_params[1].value, 0, 32));
//        LOG_DEBUG("MDR217: start       0x%"PRIX32"", buf_get_u32(reg_params[2].value, 0, 32));
        LOG_DEBUG("MDR217: address     0x%"PRIX32"", buf_get_u32(reg_params[3].value, 0, 32));

        if (retval == ERROR_FLASH_OPERATION_FAILED) {
            LOG_ERROR("MDR217: flash write failed at address 0x%"PRIX32,
                    buf_get_u32(reg_params[3].value, 0, 32));
                    break;
            }
        address += bytes_to_write;
        count   -= bytes_to_write;
        buffer  += bytes_to_write;
    } // while
free_buffer:
    target_free_working_area(target, source);
    target_free_working_area(target, hw_algorithm);

    destroy_reg_param(&reg_params[0]);
    destroy_reg_param(&reg_params[1]);
    destroy_reg_param(&reg_params[2]);
    destroy_reg_param(&reg_params[3]);

    return retval;
}

// uint32_t ConvertAddr (uint32_t mem_type, uint32_t addr){
//   uint32_t cell_addr;
//   uint32_t word_addr;
//   uint32_t new_addr;

//   cell_addr=addr/8;
//   word_addr=addr/4;

//   if (word_addr%2==0)  //from flash1 ip
//   {
//     new_addr=cell_addr*4;
//   } else {             //from flash2 ip
//     new_addr=cell_addr*4 + (mem_type? 0x00002000 : 0x00040000);
//   }

// return new_addr;
// }

static int mdr217_write(struct flash_bank *bank, const uint8_t *buffer,
        uint32_t offset, uint32_t count)
{
    struct target *target = bank->target;
    struct mdr217_flash_bank *mdr217_info = bank->driver_priv;
    uint8_t *new_buffer = NULL;

    if (bank->target->state != TARGET_HALTED) {
        LOG_ERROR("MDR217: Target not halted");
        return ERROR_TARGET_NOT_HALTED;
    }

    if (offset % 4) {
        LOG_ERROR("MDR217: offset 0x%" PRIx32 " breaks required 4-byte alignment", offset);
        return ERROR_FLASH_DST_BREAKS_ALIGNMENT;
    }

    /* If there's an odd number of bytes, the data has to be padded. Duplicate
     * the buffer and use the normal code path with a single block write since
     * it's probably cheaper than to special case the last odd write using
     * discrete accesses. */
    int rem = -count % 4;
    if (rem) {
        new_buffer = malloc(count + rem);
        if (new_buffer == NULL) {
            LOG_ERROR("MDR217: Write count breaks required 4-byte alignment and no memory for padding buffer");
            return ERROR_FAIL;
        }
        LOG_INFO("MDR217: Write count breaks required 4-byte alignment, padding with 0xff");
        buffer = memcpy(new_buffer, buffer, count);
        while (rem--)
            new_buffer[count++] = 0xff;
    }
    assert((offset % 4)==0);
    assert((count % 4)==0);

    uint32_t flash_cmd, cur_per_clock;
    int retval, retval2;

    retval = target_read_u32(target, MD_PER2_CLOCK, &cur_per_clock);
    if (retval != ERROR_OK)
        goto free_buffer;

    if (!(cur_per_clock & MD_PER2_CLOCK_RST_CLK)) {
        /* Something's very wrong if the RST_CLK module is not clocked */
        LOG_ERROR("MDR217: Target needs reset before flash operations");
        retval = ERROR_FLASH_OPERATION_FAILED;
        goto free_buffer;
    }

    retval = target_write_u32(target, MD_PER2_CLOCK, cur_per_clock | MD_PER2_CLOCK_FLASH);
    if (retval != ERROR_OK)
        goto free_buffer;

    retval = target_write_u32(target, FLASH_KEY, KEY);
    if (retval != ERROR_OK)
        goto free_buffer;

    retval = target_read_u32(target, FLASH_CMD, &flash_cmd);
    if (retval != ERROR_OK)
        goto reset_pg_and_lock;

    /* Switch on register access */
    flash_cmd = (flash_cmd & FLASH_DELAY_MASK) | FLASH_CON | FLASH_TMR;
    if (mdr217_info->mem_type)
        flash_cmd |= FLASH_IFREN;
    retval = target_write_u32(target, FLASH_CMD, flash_cmd);
    if (retval != ERROR_OK)
        goto reset_pg_and_lock;

    retval = target_write_u32(target, FLASH_CTRL, 0);
    if (retval != ERROR_OK)
        goto reset_pg_and_lock;

    unsigned int page_size = bank->size / mdr217_info->page_count;
    LOG_INFO ("MDR217: PROGRAM %s %"PRId32" bytes at offset 0x%"PRIX32"", mdr217_info->mem_type?"INFO":"MAIN", count, offset);
    /* try using hw write */
    retval = mdr217_write_hw(bank, buffer, offset, count);

    if (retval == ERROR_TARGET_RESOURCE_NOT_AVAILABLE) {
        /* if hw write failed (no sufficient working area),
         * we use normal (slow) single word accesses */
        LOG_WARNING("MDR217: Can't use hw writes, falling back to single memory accesses");
        LOG_DEBUG("MDR217: bank_size 0x%"PRIX32" page_size 0x%"PRIX32" page_count 0x%"PRIX32" sec_count 0x%"PRIX32"", bank->size, page_size, mdr217_info->page_count, mdr217_info->sec_count);

        while (count > 0) {
            // unsigned int i, j;
            unsigned int page_mask = page_size - 1;
            unsigned int page_start = offset & ~page_mask;
            unsigned int bytes_to_write = page_start + page_size - offset;
            if (count < bytes_to_write)
                bytes_to_write = count;

            //LOG_DEBUG("Selecting next page: %08x", page_start);

            // for (i = 0; i < mdr217_info->sec_count; i++) {
                retval = target_write_u32(target, FLASH_ADR, offset);
                if (retval != ERROR_OK)
                    goto reset_pg_and_lock;
                LOG_DEBUG("Programming bytes: %08x..%08x", offset, offset+bytes_to_write-1);

                flash_cmd |= FLASH_XE | FLASH_PROG;
                retval = target_write_u32(target, FLASH_CMD, flash_cmd);
                if (retval != ERROR_OK)
                    goto reset_pg_and_lock;
                usleep(FLASH_Tnvs);
                flash_cmd |= FLASH_NVSTR;
                retval = target_write_u32(target, FLASH_CMD, flash_cmd);
                if (retval != ERROR_OK)
                    goto reset_pg_and_lock;

                while (bytes_to_write) {
                    uint32_t value;
                    memcpy(&value, buffer, sizeof(uint32_t));
                    retval = target_write_u32(target, FLASH_DI, value);
                    if (retval != ERROR_OK)
                        goto reset_pg_and_lock;
                    //LOG_DEBUG("Writing to addr %08x", offset);
                    retval = target_write_u32(target, FLASH_ADR, offset);
                    if (retval != ERROR_OK)
                        goto reset_pg_and_lock;
                    usleep(FLASH_Tpgs);
                    flash_cmd |= FLASH_YE;
                    retval = target_write_u32(target, FLASH_CMD, flash_cmd);
                    if (retval != ERROR_OK)
                        goto reset_pg_and_lock;
                    usleep(FLASH_Tprog);
                    flash_cmd &= ~FLASH_YE;
                    retval = target_write_u32(target, FLASH_CMD, flash_cmd);
                    if (retval != ERROR_OK)
                        goto reset_pg_and_lock;
                    offset += 4;
                    buffer += 4;
                    count -= 4;
                    bytes_to_write -= 4;
                } // while (bytes_to_write)
                flash_cmd &= ~FLASH_PROG;
                retval = target_write_u32(target, FLASH_CMD, flash_cmd);
                if (retval != ERROR_OK)
                    goto reset_pg_and_lock;
                usleep(FLASH_Tnvh);
                flash_cmd &= ~(FLASH_XE | FLASH_NVSTR);
                retval = target_write_u32(target, FLASH_CMD, flash_cmd);
                if (retval != ERROR_OK)
                    goto reset_pg_and_lock;
                usleep(FLASH_Trcv);
            // }

            buffer += bytes_to_write;
            offset += bytes_to_write;
            count -= bytes_to_write;
        } // while (count > 0)
    } // if (retval == ERROR_TARGET_RESOURCE_NOT_AVAILABLE)

reset_pg_and_lock:
    flash_cmd = (flash_cmd & FLASH_DELAY_MASK) | FLASH_TMR;
    retval2 = target_write_u32(target, FLASH_CMD, flash_cmd);
    if (retval == ERROR_OK)
        retval = retval2;

    retval2 = target_write_u32(target, FLASH_KEY, 0);
    if (retval == ERROR_OK)
        retval = retval2;

free_buffer:
    //if (new_buffer)
        free(new_buffer);

    /* read some bytes bytes to flush buffer in flash accelerator.
     * See errata for 1986VE1T and 1986VE3. Error 0007 */
    //if ((retval == ERROR_OK) && (!mdr217_info->mem_type)) {
    //    uint32_t tmp;
    //    target_checksum_memory(bank->target, bank->base, 64, &tmp);
    //}

    return retval;
} // mdr217_write

static int __attribute__((unused)) mdr217_read(struct flash_bank *bank, uint8_t *buffer,
            uint32_t offset, uint32_t count)
{
    struct target *target = bank->target;
    struct mdr217_flash_bank *mdr217_info = bank->driver_priv;
    int retval, retval2;

    LOG_DEBUG("MDR217: offset 0x%"PRIx32" count 0x%"PRIx32"", offset, count);

    if (!mdr217_info->mem_type)
        return default_flash_read(bank, buffer, offset, count);

    if (bank->target->state != TARGET_HALTED) {
        LOG_ERROR("Target not halted");
        return ERROR_TARGET_NOT_HALTED;
    }

    if (offset % 4) {
        LOG_ERROR("offset 0x%" PRIx32 " breaks required 4-byte alignment", offset);
        return ERROR_FLASH_DST_BREAKS_ALIGNMENT;
    }

    if (count % 4) {
        LOG_ERROR("count 0x%" PRIx32 " breaks required 4-byte alignment", count);
        return ERROR_FLASH_DST_BREAKS_ALIGNMENT;
    }

    uint32_t flash_cmd, cur_per_clock;

    retval = target_read_u32(target, MD_PER2_CLOCK, &cur_per_clock);
    if (retval != ERROR_OK)
        goto err;

    if (!(cur_per_clock & MD_PER2_CLOCK_RST_CLK)) {
        /* Something's very wrong if the RST_CLK module is not clocked */
        LOG_ERROR("Target needs reset before flash operations");
        retval = ERROR_FLASH_OPERATION_FAILED;
        goto err;
    }

    retval = target_write_u32(target, MD_PER2_CLOCK, cur_per_clock | MD_PER2_CLOCK_FLASH);
    if (retval != ERROR_OK)
        goto err;

    retval = target_write_u32(target, FLASH_KEY, KEY);
    if (retval != ERROR_OK)
        goto err;

    retval = target_read_u32(target, FLASH_CMD, &flash_cmd);
    if (retval != ERROR_OK)
        goto err_lock;

    /* Switch on register access */
    flash_cmd = (flash_cmd & FLASH_DELAY_MASK) | FLASH_CON | FLASH_TMR | FLASH_IFREN;
    retval = target_write_u32(target, FLASH_CMD, flash_cmd);
    if (retval != ERROR_OK)
        goto reset_pg_and_lock;

    for (uint32_t i = 0; i < count; i += 4) {
        retval = target_write_u32(target, FLASH_ADR, offset + i);
        if (retval != ERROR_OK)
            goto reset_pg_and_lock;

        retval = target_write_u32(target, FLASH_CMD, flash_cmd |
                      FLASH_XE | FLASH_YE | FLASH_SE);
        if (retval != ERROR_OK)
            goto reset_pg_and_lock;

        uint32_t buf;
        retval = target_read_u32(target, FLASH_DO, &buf);
        if (retval != ERROR_OK)
            goto reset_pg_and_lock;

        buf_set_u32(buffer, i * 8, 32, buf);

        retval = target_write_u32(target, FLASH_CMD, flash_cmd);
        if (retval != ERROR_OK)
            goto reset_pg_and_lock;

    }

reset_pg_and_lock:
    flash_cmd &= FLASH_DELAY_MASK;
    retval2 = target_write_u32(target, FLASH_CMD, flash_cmd);
    if (retval == ERROR_OK)
        retval = retval2;

err_lock:
    retval2 = target_write_u32(target, FLASH_KEY, 0);
    if (retval == ERROR_OK)
        retval = retval2;

err:
    return retval;
}

static int mdr217_probe(struct flash_bank *bank)
{
    struct mdr217_flash_bank *mdr217_info = bank->driver_priv;
    unsigned int sect_count, sect_size, i;

    sect_count = mdr217_info->sec_count;
    sect_size = bank->size / sect_count;

    free(bank->sectors);

    bank->num_sectors = sect_count;
    bank->sectors = malloc(sizeof(struct flash_sector) * sect_count);

    for (i = 0; i < sect_count; i++) {
        bank->sectors[i].offset = i * sect_size;
        bank->sectors[i].size = sect_size;
        bank->sectors[i].is_erased = -1;
        bank->sectors[i].is_protected = 0;
    }

    mdr217_info->probed = true;

    return ERROR_OK;
}

static int mdr217_auto_probe(struct flash_bank *bank)
{
    struct mdr217_flash_bank *mdr217_info = bank->driver_priv;
    if (mdr217_info->probed)
        return ERROR_OK;
    return mdr217_probe(bank);
}

int get_mdr217_info(struct flash_bank *bank, struct command_invocation *cmd)
{
    struct mdr217_flash_bank *mdr217_info = bank->driver_priv;
    command_print_sameline(cmd, "MDR217 - %s",
         mdr217_info->mem_type ? "info memory" : "main memory");

    return ERROR_OK;
}

int get_mdr217_info_OLDAPI(struct flash_bank *bank, char *buf, int buf_size)
{
    struct mdr217_flash_bank *mdr217_info = bank->driver_priv;
    snprintf(buf, buf_size, "MDR217 - %s",
         mdr217_info->mem_type ? "info memory" : "main memory");

    return ERROR_OK;
}

const struct flash_driver mdr217_flash = {
    .name = "mdr217",
    .usage = "flash bank <name> mdr217 <base> <size> 0 0 <target#> <type> <page_count> <sec_count>"
    "<type>: 0 for main memory, 1 for info memory",
    .flash_bank_command = mdr217_flash_bank_command,
    .erase = mdr217_erase,
    .write = mdr217_write,
    .read = default_flash_read, //mdr217_read,
    .probe = mdr217_probe,
    .auto_probe = mdr217_auto_probe,
    .erase_check = default_flash_blank_check,
    .info = __builtin_choose_expr (__builtin_types_compatible_p(typeof(&get_mdr217_info), typeof(mdr217_flash.info)), get_mdr217_info, get_mdr217_info_OLDAPI),
    .free_driver_priv = default_flash_free_driver_priv,
};
