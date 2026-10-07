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

#define FLASH_TMEN       (1 << 14)  // Test Memory Reset
#define FLASH_PROG2      (1 << 13)
#define FLASH_PROG       (1 << 12)
#define FLASH_CHIP       (1 << 11)
#define FLASH_ERASE      (1 << 10)
#define FLASH_NVR        (1 << 9)
#define FLASH_WE         (1 << 7)
#define FLASH_CE         (1 << 6)
#define FLASH_CON        (1 << 0)
#define FLASH_DELAY_MASK (1 << 3)

#define MLDR215_FLASH_TERASE_CHIP       35000 // 30000 .. 40000
#define MLDR215_FLASH_TERASE_SECT        2500 // 2000 .. 3000
#define MLDR215_FLASH_TNVS_CHIP_ERASE   80
#define MLDR215_FLASH_TNVS_SECT_ERASE   20
#define MLDR215_FLASH_TNVS_PROG         20
#define MLDR215_FLASH_TPROG             6 // 5 .. 6.5
#define MLDR215_FLASH_TPGS              60 // 50..70
#define MLDR215_FLASH_TRCV_CHIP_ERASE   200
#define MLDR215_FLASH_TRCV_SECT_ERASE   50
#define MLDR215_FLASH_TRCV_PROG         50
#define MLDR215_FLASH_TRW1              1 // 0.5..
#define MLDR215_FLASH_TRW2              10

#define KEY     0x8AAA5551

//#define UNUSED(x) (void)(x) // __attribute__((unused))

struct mdr215_flash_bank {
    int probed;
    unsigned int mem_type;
    unsigned int page_count;
    unsigned int sec_count;
};

/* see ... for src */
static const unsigned char mdr215_flash_write_code[] =
{
0x37, 0x57, 0xAA, 0x8A, 0xB7, 0x87, 0x01, 0x50, 0x13, 0x07, 0x17, 0x55,
0x98, 0xCB, 0x03, 0xA3, 0x07, 0x00, 0x91, 0x68, 0x85, 0x08, 0x13, 0x73,
0x83, 0x00, 0xB3, 0x68, 0x13, 0x01, 0x23, 0xA0, 0x17, 0x01, 0x23, 0xAA,
0x07, 0x00, 0xB7, 0x0F, 0x04, 0x00, 0x11, 0xC1, 0x89, 0x6F, 0x95, 0x67,
0x13, 0x1E, 0x95, 0x00, 0x33, 0x6E, 0x6E, 0x00, 0x93, 0x8E, 0x17, 0x04,
0x93, 0x87, 0x17, 0x0C, 0x33, 0x68, 0xFE, 0x00, 0x11, 0x6F, 0x9D, 0x67,
0x13, 0x0F, 0x1F, 0x04, 0x93, 0x87, 0x17, 0x0C, 0xB3, 0x6E, 0xDE, 0x01,
0x33, 0x6F, 0xEE, 0x01, 0x33, 0x6E, 0xFE, 0x00, 0x93, 0xD7, 0x36, 0x00,
0x13, 0xF7, 0x46, 0x00, 0x8A, 0x07, 0x11, 0xC3, 0xFE, 0x97, 0x37, 0x87,
0x01, 0x50, 0x5C, 0xC3, 0x23, 0x20, 0xD7, 0x01, 0x73, 0x27, 0x00, 0xB0,
0x13, 0x07, 0x07, 0x0A, 0xF3, 0x27, 0x00, 0xB0, 0x99, 0x8F, 0xE3, 0xCD,
0x07, 0xFE, 0xB7, 0x87, 0x01, 0x50, 0x23, 0xA0, 0x07, 0x01, 0x73, 0x27,
0x00, 0xB0, 0x13, 0x07, 0x07, 0x1E, 0x01, 0x00, 0xF3, 0x27, 0x00, 0xB0,
0x99, 0x8F, 0xE3, 0xCD, 0x07, 0xFE, 0x13, 0xF5, 0x36, 0x00, 0xB3, 0x82,
0xC6, 0x40, 0x01, 0x00, 0x05, 0x47, 0xB7, 0x87, 0x01, 0x50, 0x33, 0x17,
0xA7, 0x00, 0xD8, 0xCB, 0x05, 0x06, 0x03, 0x47, 0xF6, 0xFF, 0x93, 0x16,
0x35, 0x00, 0x33, 0x17, 0xD7, 0x00, 0x98, 0xC7, 0x01, 0x00, 0x01, 0x00,
0x01, 0x00, 0x01, 0x00, 0x23, 0xA0, 0xC7, 0x01, 0x73, 0x27, 0x00, 0xB0,
0x13, 0x07, 0x07, 0x03, 0xF3, 0x27, 0x00, 0xB0, 0x99, 0x8F, 0xE3, 0xCD,
0x07, 0xFE, 0xB7, 0x87, 0x01, 0x50, 0x23, 0xA0, 0x07, 0x01, 0x01, 0x00,
0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0xFD, 0x15, 0xB3, 0x06, 0x56, 0x00,
0x89, 0xC5, 0x05, 0x05, 0x91, 0x47, 0xE3, 0x15, 0xF5, 0xFA, 0xB7, 0x87,
0x01, 0x50, 0x23, 0xA0, 0xD7, 0x01, 0x73, 0x27, 0x00, 0xB0, 0x13, 0x07,
0x07, 0x19, 0x01, 0x00, 0xF3, 0x27, 0x00, 0xB0, 0x99, 0x8F, 0xE3, 0xCD,
0x07, 0xFE, 0xB7, 0x87, 0x01, 0x50, 0x23, 0xA0, 0xE7, 0x01, 0x23, 0xA0,
0x17, 0x01, 0x73, 0x27, 0x00, 0xB0, 0x21, 0x07, 0xF3, 0x27, 0x00, 0xB0,
0x99, 0x8F, 0xE3, 0xCD, 0x07, 0xFE, 0x99, 0xFD, 0x11, 0x67, 0xB7, 0x87,
0x01, 0x50, 0x33, 0x63, 0xE3, 0x00, 0x23, 0xA0, 0x67, 0x00, 0x23, 0xAA,
0x07, 0x00, 0x23, 0xA8, 0x07, 0x00, 0x93, 0x07, 0xA0, 0x02, 0x3E, 0x85,
0xAE, 0x85, 0xB6, 0x86, 0x02, 0x90, 0x82, 0x80, 0x00, 0x00,
};

static const unsigned char mdr215_flash_mass_erase[] =
{
0x37, 0x57, 0xAA, 0x8A, 0xB7, 0x87, 0x01, 0x50, 0x13, 0x07, 0x17, 0x55,
0x98, 0xCB, 0x94, 0x43, 0x11, 0x66, 0x13, 0x08, 0x16, 0x00, 0xA1, 0x8A,
0x33, 0xE8, 0x06, 0x01, 0x23, 0xA0, 0x07, 0x01, 0x95, 0x65, 0x13, 0x85,
0x15, 0xC4, 0x23, 0xAA, 0x07, 0x00, 0x93, 0x85, 0x15, 0xCC, 0x13, 0x06,
0x16, 0x04, 0x23, 0xA2, 0x07, 0x00, 0x55, 0x8D, 0xD5, 0x8D, 0x55, 0x8E,
0x89, 0x48, 0xB7, 0x87, 0x01, 0x50, 0x88, 0xC3, 0x73, 0x27, 0x00, 0xB0,
0x13, 0x07, 0x07, 0x28, 0xF3, 0x27, 0x00, 0xB0, 0x99, 0x8F, 0xE3, 0xCD,
0x07, 0xFE, 0xB7, 0x87, 0x01, 0x50, 0x8C, 0xC3, 0x73, 0x27, 0x00, 0xB0,
0xB7, 0x47, 0x04, 0x00, 0x93, 0x87, 0x07, 0x5C, 0x3E, 0x97, 0x01, 0x00,
0xF3, 0x27, 0x00, 0xB0, 0x99, 0x8F, 0xE3, 0xCD, 0x07, 0xFE, 0xB7, 0x87,
0x01, 0x50, 0x88, 0xC3, 0x73, 0x27, 0x00, 0xB0, 0x13, 0x07, 0x07, 0x64,
0xF3, 0x27, 0x00, 0xB0, 0x99, 0x8F, 0xE3, 0xCD, 0x07, 0xFE, 0xB7, 0x87,
0x01, 0x50, 0x90, 0xC3, 0x23, 0xA0, 0x07, 0x01, 0x73, 0x27, 0x00, 0xB0,
0x13, 0x07, 0x07, 0x05, 0xF3, 0x27, 0x00, 0xB0, 0x99, 0x8F, 0xE3, 0xCD,
0x07, 0xFE, 0xB7, 0x87, 0x01, 0x50, 0xD8, 0x43, 0x37, 0x0E, 0x04, 0x00,
0x05, 0x43, 0x72, 0x97, 0xD8, 0xC3, 0x63, 0x9D, 0x68, 0x00, 0x11, 0x67,
0xD9, 0x8E, 0x94, 0xC3, 0x23, 0xA8, 0x07, 0x00, 0x93, 0x07, 0xE0, 0x0E,
0x3E, 0x85, 0x02, 0x90, 0x82, 0x80, 0x01, 0x00, 0x85, 0x48, 0xA5, 0xB7,
};

static const unsigned char mdr215_flash_erase_sect[] =
{
0xB7, 0x57, 0xAA, 0x8A, 0x37, 0x87, 0x01, 0x50, 0x93, 0x87, 0x17, 0x55,
0x1C, 0xCB, 0x03, 0x2E, 0x07, 0x00, 0x91, 0x67, 0x13, 0x83, 0x17, 0x00,
0x13, 0x7E, 0x8E, 0x00, 0x33, 0x63, 0x6E, 0x00, 0x23, 0x20, 0x67, 0x00,
0x23, 0x2A, 0x07, 0x00, 0x63, 0x67, 0xB6, 0x0C, 0x93, 0x18, 0x95, 0x00,
0xB3, 0xE8, 0xC8, 0x01, 0x13, 0x88, 0x17, 0x44, 0x93, 0x87, 0x17, 0x4C,
0x33, 0xE8, 0x08, 0x01, 0xB7, 0x0E, 0x04, 0x00, 0xB3, 0xE8, 0xF8, 0x00,
0x61, 0xE9, 0x11, 0x65, 0x05, 0x06, 0x13, 0x05, 0x15, 0x04, 0x93, 0x96,
0xA5, 0x00, 0x33, 0x65, 0xAE, 0x00, 0x93, 0x15, 0xA6, 0x00, 0x01, 0x00,
0x93, 0xD7, 0x16, 0x00, 0x37, 0x87, 0x01, 0x50, 0x5C, 0xC3, 0x09, 0x46,
0xB7, 0x87, 0x01, 0x50, 0x23, 0xA0, 0x07, 0x01, 0x73, 0x27, 0x00, 0xB0,
0x13, 0x07, 0x07, 0x0A, 0xF3, 0x27, 0x00, 0xB0, 0x99, 0x8F, 0xE3, 0xCD,
0x07, 0xFE, 0xB7, 0x87, 0x01, 0x50, 0x23, 0xA0, 0x17, 0x01, 0x73, 0x27,
0x00, 0xB0, 0x95, 0x67, 0x93, 0x87, 0x07, 0xE2, 0x3E, 0x97, 0x01, 0x00,
0xF3, 0x27, 0x00, 0xB0, 0x99, 0x8F, 0xE3, 0xCD, 0x07, 0xFE, 0xB7, 0x87,
0x01, 0x50, 0x23, 0xA0, 0x07, 0x01, 0x73, 0x27, 0x00, 0xB0, 0x13, 0x07,
0x07, 0x19, 0x01, 0x00, 0xF3, 0x27, 0x00, 0xB0, 0x99, 0x8F, 0xE3, 0xCD,
0x07, 0xFE, 0xB7, 0x87, 0x01, 0x50, 0x88, 0xC3, 0x23, 0xA0, 0x67, 0x00,
0x73, 0x27, 0x00, 0xB0, 0x21, 0x07, 0x01, 0x00, 0xF3, 0x27, 0x00, 0xB0,
0x99, 0x8F, 0xE3, 0xCD, 0x07, 0xFE, 0x37, 0x87, 0x01, 0x50, 0x5C, 0x43,
0x05, 0x4F, 0xF6, 0x97, 0x5C, 0xC3, 0x63, 0x15, 0xE6, 0x03, 0x93, 0x86,
0x06, 0x40, 0xE3, 0x97, 0xD5, 0xF6, 0x91, 0x67, 0x37, 0x87, 0x01, 0x50,
0x33, 0x6E, 0xFE, 0x00, 0x23, 0x20, 0xC7, 0x01, 0x23, 0x28, 0x07, 0x00,
0x93, 0x07, 0xE0, 0x0E, 0x3E, 0x85, 0x02, 0x90, 0x82, 0x80, 0x01, 0x00,
0x05, 0x46, 0x99, 0xBF, 0x89, 0x6E, 0x05, 0xBF,
};

/* flash bank <name> mdr <base> <size> 0 0 <target#> <type> <page_count> <sec_count> */
FLASH_BANK_COMMAND_HANDLER(mdr215_flash_bank_command)
{
    struct mdr215_flash_bank *mdr215_info;

    if (CMD_ARGC < 9)
        return ERROR_COMMAND_SYNTAX_ERROR;

    mdr215_info = malloc(sizeof(struct mdr215_flash_bank));

    bank->driver_priv = mdr215_info;
    mdr215_info->probed = false;
    COMMAND_PARSE_NUMBER(uint, CMD_ARGV[6], mdr215_info->mem_type);
    COMMAND_PARSE_NUMBER(uint, CMD_ARGV[7], mdr215_info->page_count);
    COMMAND_PARSE_NUMBER(uint, CMD_ARGV[8], mdr215_info->sec_count);
    LOG_DEBUG("MDR215: flash bank");
    return ERROR_OK;
}

static int mdr215_flash_mass_erase_hw(struct flash_bank *bank)
{
    struct target *target = bank->target;
    struct working_area *hw_algorithm;
    struct reg_param reg_params[1];
    int retval = ERROR_OK;

//    LOG_INFO ("MDR215: ERASE CHIP HW @0x%"PRIX32"", (uint32_t)bank->base);
//
//  FILE *fp = NULL;
//  ssize_t r;
//  fp = fopen("mdr215_flash_mass_erase.bin", "wb");
//  r = fwrite(mdr215_flash_mass_erase, 1, sizeof(mdr215_flash_mass_erase), fp);
//  LOG_DEBUG("MDR215: file len %"PRId32" ", (unsigned int)r);
//  fclose(fp);

    /* flash erase code */
    LOG_DEBUG("MDR215: request %"PRId32" bytes of memory", (unsigned int)sizeof(mdr215_flash_mass_erase));
    if (sizeof(mdr215_flash_mass_erase)==0 || target_alloc_working_area(target, sizeof(mdr215_flash_mass_erase),
            &hw_algorithm) != ERROR_OK) {
        //LOG_WARNING("MDR215: no working area available, can't do hw memory erase");
        return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
    }

    retval = target_write_u32(target, MD_CPU_CLOCK, (1<<8) |(0<<4) |(0<<2) |(0<<0)); // CPU_C1=CPU_C2=CPU_C3=HCLK = HSI ~8MHz
    if (retval != ERROR_OK)
        goto free_buffer;

    retval = target_write_buffer(target, hw_algorithm->address,
            sizeof(mdr215_flash_mass_erase), mdr215_flash_mass_erase);
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

    LOG_DEBUG("MDR215: status      0x%"PRIX32"", buf_get_u32(reg_params[0].value, 0, 32));

    if (retval == ERROR_FLASH_OPERATION_FAILED) {
        LOG_ERROR("MDR215: flash erase failed");
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

static int mdr215_flash_erase_sect_hw(struct flash_bank *bank, unsigned int first, unsigned int last)
{
    struct target *target = bank->target;
    struct mdr215_flash_bank *mdr215_info = bank->driver_priv;
    struct working_area *hw_algorithm;
    struct reg_param reg_params[3];
    int retval = ERROR_OK;

    /* flash erase code */
    LOG_DEBUG("MDR215: request %"PRId32" bytes of memory", (unsigned int)sizeof(mdr215_flash_erase_sect));
    if (sizeof(mdr215_flash_erase_sect)==0 || target_alloc_working_area(target, sizeof(mdr215_flash_erase_sect),
            &hw_algorithm) != ERROR_OK) {
        //LOG_WARNING("MDR215: no working area available, can't do hw memory erase");
        return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
    }

    retval = target_write_u32(target, MD_CPU_CLOCK, (1<<8) |(0<<4) |(0<<2) |(0<<0)); // CPU_C1=CPU_C2=CPU_C3=HCLK = HSI ~8MHz
    if (retval != ERROR_OK)
        goto free_buffer;

    retval = target_write_buffer(target, hw_algorithm->address,
            sizeof(mdr215_flash_erase_sect), mdr215_flash_erase_sect);
    if (retval != ERROR_OK)
        goto free_buffer;

    // a0=x10 s10=x26
    init_reg_param(&reg_params[0], "a0", 32, PARAM_IN_OUT); /* mem_type (PC->uC), status (uC->PC) */
    init_reg_param(&reg_params[1], "a1", 32, PARAM_OUT);    /* first (PC->uC)*/
    init_reg_param(&reg_params[2], "a2", 32, PARAM_OUT);    /* last (PC->uC) */

    buf_set_u32(reg_params[0].value, 0, 32, mdr215_info->mem_type);
    buf_set_u32(reg_params[1].value, 0, 32, first);
    buf_set_u32(reg_params[2].value, 0, 32, last);

    LOG_DEBUG("MDR215: mem_type       0x%"PRIX32"", buf_get_u32(reg_params[0].value, 0, 32));
    LOG_DEBUG("MDR215: first          0x%"PRIX32"", buf_get_u32(reg_params[1].value, 0, 32));
    LOG_DEBUG("MDR215: last           0x%"PRIX32"", buf_get_u32(reg_params[2].value, 0, 32));

    //int target_run_algorithm(struct target *target,
    //      int num_mem_params, struct mem_param *mem_params,
    //      int num_reg_params, struct reg_param *reg_param,
    //      uint32_t entry_point, uint32_t exit_point,
    //      int timeout_ms, void *arch_info)

    retval = target_run_algorithm(target,
            0, NULL,
            3, reg_params,
            hw_algorithm->address, 0,
            1000+12*(last-first+1), NULL); // 3ms*2chip*2tol

    LOG_DEBUG("MDR215: status      0x%"PRIX32"", buf_get_u32(reg_params[0].value, 0, 32));

    if (retval == ERROR_FLASH_OPERATION_FAILED) {
        LOG_ERROR("MDR215: flash erase failed");
        }

    destroy_reg_param(&reg_params[0]);
    destroy_reg_param(&reg_params[1]);
    destroy_reg_param(&reg_params[2]);

free_buffer:
    target_free_working_area(target, hw_algorithm);

    return retval;
}

static int mdr215_mass_erase(struct flash_bank *bank) // MAIN memory only
{
    struct target *target = bank->target;
    struct mdr215_flash_bank *mdr215_info = bank->driver_priv;
    uint32_t flash_cmd;
    int retval, retval2;

    LOG_INFO ("MDR215: MASS ERASE %s", mdr215_info->mem_type?"INFO":"MAIN"); // , (uint32_t)bank->base

    /* try using hw erase */
    retval = mdr215_flash_mass_erase_hw(bank);

    if (retval == ERROR_TARGET_RESOURCE_NOT_AVAILABLE) {
        /* if hw erase failed (no sufficient working area),
         * we use normal (slow) single word accesses */
        LOG_WARNING("MDR215: Can't use hw erase, falling back to single memory accesses");

        retval = target_read_u32(target, FLASH_CMD, &flash_cmd);
        if (retval != ERROR_OK)
            return retval;

        /* Switch on register access */
        flash_cmd = (flash_cmd & FLASH_DELAY_MASK) | FLASH_CON | FLASH_TMEN;
        retval = target_write_u32(target, FLASH_CMD, flash_cmd);
        if (retval != ERROR_OK)
            goto reset_pg_and_lock;

        for(uint32_t chip=0;chip<2;chip++) {
            uint32_t addr = (mdr215_info->mem_type? 0x00002000 : 0x00040000)*chip;
            LOG_DEBUG("MDR215: MASS ERASE ADR=0x%"PRIX32"", addr);

            retval = target_write_u32(target, FLASH_ADR, addr);
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;

            flash_cmd |= FLASH_CE | FLASH_ERASE | FLASH_CHIP;
            retval = target_write_u32(target, FLASH_CMD, flash_cmd);
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;
            usleep(MLDR215_FLASH_TNVS_CHIP_ERASE);
            flash_cmd |= FLASH_WE;
            retval = target_write_u32(target, FLASH_CMD, flash_cmd);
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;
            usleep(MLDR215_FLASH_TERASE_CHIP);
            flash_cmd &= ~FLASH_WE;
            retval = target_write_u32(target, FLASH_CMD, flash_cmd);
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;
            usleep(MLDR215_FLASH_TRCV_CHIP_ERASE);
            flash_cmd &= ~FLASH_ERASE;
            retval = target_write_u32(target, FLASH_CMD, flash_cmd);
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;
            flash_cmd &= ~FLASH_CE;
            retval = target_write_u32(target, FLASH_CMD, flash_cmd);
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;
            usleep(MLDR215_FLASH_TRW2);
        } // for chip
        for (uint32_t sect = 0; sect < bank->num_sectors; sect++) {
            bank->sectors[sect].is_erased = 1; //?
        } // for sect
    } // TARGET_RESOURCE_NOT_AVAILABLE

reset_pg_and_lock:
    flash_cmd = (flash_cmd & FLASH_DELAY_MASK) | FLASH_TMEN;
    retval2 = target_write_u32(target, FLASH_CMD, flash_cmd);
    if (retval == ERROR_OK)
        retval = retval2;

    retval2 = target_write_u32(target, FLASH_KEY, 0x0);
    if (retval == ERROR_OK)
        retval = retval2;

    return retval;
} // mdr215_mass_erase

static int mdr215_erase(struct flash_bank *bank, unsigned int first, unsigned int last)
{
    struct target *target = bank->target;
    struct mdr215_flash_bank *mdr215_info = bank->driver_priv;
    int retval, retval2;
    uint32_t flash_cmd, cur_per_clock;

    if (bank->target->state != TARGET_HALTED) {
        LOG_ERROR("MDR215: Target not halted");
        return ERROR_TARGET_NOT_HALTED;
    }

    retval = target_read_u32(target, MD_PER2_CLOCK, &cur_per_clock);
    if (retval != ERROR_OK)
        return retval;

    retval = target_write_u32(target, MD_PER2_CLOCK, cur_per_clock | MD_PER2_CLOCK_FLASH);
    if (retval != ERROR_OK)
        return retval;

    if (!(cur_per_clock & MD_PER2_CLOCK_RST_CLK)) {
        LOG_ERROR("MDR215: Target needs reset before flash operations");
        return ERROR_FLASH_OPERATION_FAILED;
    }

    retval = target_write_u32(target, FLASH_KEY, KEY);
    if (retval != ERROR_OK)
        return retval;

    if ((first == 0) && (last >= (bank->num_sectors - 1)) &&
        !mdr215_info->mem_type) {
        retval = mdr215_mass_erase(bank);
        goto reset_pg_and_lock;
    }

    LOG_INFO ("MDR215: ERASE %s sectors from %"PRId32" to %"PRId32"", mdr215_info->mem_type?"INFO":"MAIN", first, last); // , (uint32_t)bank->base

    /* try using hw erase */
    retval = mdr215_flash_erase_sect_hw(bank, first, last);

    if (retval == ERROR_TARGET_RESOURCE_NOT_AVAILABLE) {
        /* if hw erase failed (no sufficient working area),
         * we use normal (slow) single word accesses */
        LOG_WARNING("MDR215: Can't use hw erase, falling back to single memory accesses");

        retval = target_read_u32(target, FLASH_CMD, &flash_cmd);
        if (retval != ERROR_OK)
            goto reset_pg_and_lock;

        /* Switch on register access */
        flash_cmd = (flash_cmd & FLASH_DELAY_MASK) | FLASH_CON | FLASH_TMEN;
        if (mdr215_info->mem_type)
            flash_cmd |= FLASH_NVR;

        retval = target_write_u32(target, FLASH_CMD, flash_cmd);
        if (retval != ERROR_OK)
            goto reset_pg_and_lock;

        unsigned int page_size = bank->size / mdr215_info->page_count;
        unsigned int sect_size = bank->size / mdr215_info->sec_count;
        LOG_DEBUG("MDR215: page_size 0x%"PRIX32" page_count 0x%"PRIX32" sect_size 0x%"PRIX32" sec_count 0x%"PRIX32" num_sectors 0x%"PRIX32"", page_size, mdr215_info->page_count, sect_size, mdr215_info->sec_count, bank->num_sectors);

        for (uint32_t sect = first; sect <= last; sect++) {
            for(uint32_t chip=0;chip<2;chip++) {
                uint32_t addr = ((sect * sect_size) >> 1)+(mdr215_info->mem_type? 0x00002000 : 0x00040000)*chip;
                LOG_DEBUG("MDR215: ERASE ADR=0x%"PRIX32"", addr);
                retval = target_write_u32(target, FLASH_ADR, addr);
                if (retval != ERROR_OK)
                    goto reset_pg_and_lock;

                flash_cmd |= FLASH_CE | FLASH_ERASE;
                retval = target_write_u32(target, FLASH_CMD, flash_cmd);
                if (retval != ERROR_OK)
                    goto reset_pg_and_lock;
                usleep(MLDR215_FLASH_TNVS_SECT_ERASE);
                flash_cmd |= FLASH_WE;
                retval = target_write_u32(target, FLASH_CMD, flash_cmd);
                if (retval != ERROR_OK)
                    goto reset_pg_and_lock;
                usleep(MLDR215_FLASH_TERASE_SECT);
                flash_cmd &= ~FLASH_WE;
                retval = target_write_u32(target, FLASH_CMD, flash_cmd);
                if (retval != ERROR_OK)
                    goto reset_pg_and_lock;
                usleep(MLDR215_FLASH_TRCV_SECT_ERASE);
                flash_cmd &= ~FLASH_ERASE;
                retval = target_write_u32(target, FLASH_CMD, flash_cmd);
                if (retval != ERROR_OK)
                    goto reset_pg_and_lock;
                flash_cmd &= ~FLASH_CE;
                retval = target_write_u32(target, FLASH_CMD, flash_cmd);
                if (retval != ERROR_OK)
                    goto reset_pg_and_lock;
                usleep(MLDR215_FLASH_TRW1);
            }
            bank->sectors[sect].is_erased = 1; //?
        } // for sect
    } // if (retval == ERROR_TARGET_RESOURCE_NOT_AVAILABLE)

    reset_pg_and_lock:
    flash_cmd = (flash_cmd & FLASH_DELAY_MASK) | FLASH_TMEN;
    retval2 = target_write_u32(target, FLASH_CMD, flash_cmd);
    if (retval == ERROR_OK)
        retval = retval2;

    retval2 = target_write_u32(target, FLASH_KEY, 0x0);
    if (retval == ERROR_OK)
        retval = retval2;

    return retval;
} // mdr215_erase

static int mdr215_write_hw(struct flash_bank *bank, const uint8_t *buffer,
        uint32_t offset, uint32_t count)
{
    struct target *target = bank->target;
    struct mdr215_flash_bank *mdr215_info = bank->driver_priv;
    unsigned int page_size = bank->size / mdr215_info->page_count;
    uint32_t buffer_size = page_size;
    struct working_area *hw_algorithm;
    struct working_area *source;
    uint32_t address = bank->base + offset;
    unsigned int bytes_to_write;
    struct reg_param reg_params[4];
    int retval = ERROR_OK;

    /* flash write code */
    LOG_DEBUG("MDR215: request %"PRId32" bytes of memory", (unsigned int)sizeof(mdr215_flash_write_code));
    if (sizeof(mdr215_flash_write_code)==0 || target_alloc_working_area(target, sizeof(mdr215_flash_write_code),
            &hw_algorithm) != ERROR_OK) {
        //LOG_WARNING("no working area available, can't do hw memory writes");
        return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
    }
    retval = target_write_u32(target, MD_CPU_CLOCK, (1<<8) |(0<<4) |(0<<2) |(0<<0)); // CPU_C1=CPU_C2=CPU_C3=HCLK = HSI ~8MHz
    if (retval != ERROR_OK)
        return retval;

    retval = target_write_buffer(target, hw_algorithm->address,
            sizeof(mdr215_flash_write_code), mdr215_flash_write_code);
    if (retval != ERROR_OK)
        return retval;

    bytes_to_write = buffer_size<count? buffer_size:count;// min(buffer_size, count);
    /* memory buffer */
    LOG_DEBUG("MDR215: request %"PRId32" bytes of memory", bytes_to_write);
    while (target_alloc_working_area_try(target, bytes_to_write, &source) != ERROR_OK) {
            /* we already allocated the writing code, but failed to get a
             * buffer, free the algorithm */
            target_free_working_area(target, hw_algorithm);

            //LOG_WARNING("MDR215: no large enough working area available, can't do hw memory writes");
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

        buf_set_u32(reg_params[0].value, 0, 32, mdr215_info->mem_type);
        buf_set_u32(reg_params[1].value, 0, 32, bytes_to_write);
        buf_set_u32(reg_params[2].value, 0, 32, source->address);
        buf_set_u32(reg_params[3].value, 0, 32, address);

        LOG_DEBUG("MDR215: mem_type       0x%"PRIX32"", buf_get_u32(reg_params[0].value, 0, 32));
        LOG_DEBUG("MDR215: byte_count     0x%"PRIX32"", buf_get_u32(reg_params[1].value, 0, 32));
        LOG_DEBUG("MDR215: start          0x%"PRIX32"", buf_get_u32(reg_params[2].value, 0, 32));
        LOG_DEBUG("MDR215: address        0x%"PRIX32"", buf_get_u32(reg_params[3].value, 0, 32));

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

        LOG_DEBUG("MDR215: status      0x%"PRIX32"", buf_get_u32(reg_params[0].value, 0, 32));
        LOG_DEBUG("MDR215: byte_count  0x%"PRIX32"", buf_get_u32(reg_params[1].value, 0, 32));
//        LOG_DEBUG("MDR215: start       0x%"PRIX32"", buf_get_u32(reg_params[2].value, 0, 32));
        LOG_DEBUG("MDR215: address     0x%"PRIX32"", buf_get_u32(reg_params[3].value, 0, 32));

        if (retval == ERROR_FLASH_OPERATION_FAILED) {
            LOG_ERROR("MDR215: flash write failed at address 0x%"PRIX32,
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

uint32_t ConvertAddr (uint32_t mem_type, uint32_t addr){
  uint32_t cell_addr;
  uint32_t word_addr;
  uint32_t new_addr;

  cell_addr=addr/8;
  word_addr=addr/4;

  if (word_addr%2==0)  //from flash1 ip
  {
    new_addr=cell_addr*4;
  } else {             //from flash2 ip
    new_addr=cell_addr*4 + (mem_type? 0x00002000 : 0x00040000);
  }

return new_addr;
}

static int mdr215_write(struct flash_bank *bank, const uint8_t *buffer,
        uint32_t offset, uint32_t count)
{
    struct target *target = bank->target;
    struct mdr215_flash_bank *mdr215_info = bank->driver_priv;

    if (bank->target->state != TARGET_HALTED) {
        LOG_ERROR("MDR215: Target not halted");
        return ERROR_TARGET_NOT_HALTED;
    }

    uint32_t flash_cmd, cur_per_clock;
    int retval, retval2;

    retval = target_read_u32(target, MD_PER2_CLOCK, &cur_per_clock);
    if (retval != ERROR_OK)
        goto free_buffer;

    if (!(cur_per_clock & MD_PER2_CLOCK_RST_CLK)) {
        /* Something's very wrong if the RST_CLK module is not clocked */
        LOG_ERROR("MDR215: Target needs reset before flash operations");
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
    flash_cmd = (flash_cmd & FLASH_DELAY_MASK) | FLASH_CON | FLASH_TMEN;
    if (mdr215_info->mem_type)
        flash_cmd |= FLASH_NVR;
    retval = target_write_u32(target, FLASH_CMD, flash_cmd);
    if (retval != ERROR_OK)
        goto reset_pg_and_lock;

    retval = target_write_u32(target, FLASH_CTRL, 0);
    if (retval != ERROR_OK)
        goto reset_pg_and_lock;

    unsigned int page_size = bank->size / mdr215_info->page_count;
    LOG_INFO ("MDR215: PROGRAM %s %"PRId32" bytes at offset 0x%"PRIX32"", mdr215_info->mem_type?"INFO":"MAIN", count, offset);
    /* try using hw write */
    retval = mdr215_write_hw(bank, buffer, offset, count);

    if (retval == ERROR_TARGET_RESOURCE_NOT_AVAILABLE) {
        /* if hw write failed (no sufficient working area),
         * we use normal (slow) single word accesses */
        LOG_WARNING("MDR215: Can't use hw writes, falling back to single memory accesses");
        LOG_DEBUG("MDR215: bank_size 0x%"PRIX32" page_size 0x%"PRIX32" page_count 0x%"PRIX32" sec_count 0x%"PRIX32"", bank->size, page_size, mdr215_info->page_count, mdr215_info->sec_count);

        while (count > 0) {
            uint32_t addr = ConvertAddr(mdr215_info->mem_type, offset);
            LOG_DEBUG("MDR215: WRITE ADR=0x%"PRIX32"", addr);
            retval = target_write_u32(target, FLASH_ADR, addr);
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;

            flash_cmd |= FLASH_CE | FLASH_PROG;
            retval = target_write_u32(target, FLASH_CMD, flash_cmd);
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;
            usleep(MLDR215_FLASH_TNVS_PROG); // TNVS
            flash_cmd |= FLASH_WE;
            retval = target_write_u32(target, FLASH_CMD, flash_cmd);
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;
            usleep(MLDR215_FLASH_TPGS); // TPGS

            for(int byte = offset%4; byte < 4; byte++) {
                retval = target_write_u32(target, FLASH_CTRL, (1<<byte));
                if (retval != ERROR_OK)
                    goto reset_pg_and_lock;
                retval = target_write_u32(target, FLASH_DI, (uint32_t)*buffer++ << (byte*8));
                if (retval != ERROR_OK)
                    goto reset_pg_and_lock;
                // TAds
                flash_cmd |= FLASH_PROG2;
                retval = target_write_u32(target, FLASH_CMD, flash_cmd);
                if (retval != ERROR_OK)
                    goto reset_pg_and_lock;
                usleep(MLDR215_FLASH_TPROG); // Tprog min 5us max 6.5us
                flash_cmd &= ~FLASH_PROG2;
                retval = target_write_u32(target, FLASH_CMD, flash_cmd);
                if (retval != ERROR_OK)
                    goto reset_pg_and_lock;
                // TAdh
                offset++;
                if (0 == --count)
                    break;
                }
            flash_cmd &= ~FLASH_WE;
            retval = target_write_u32(target, FLASH_CMD, flash_cmd);
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;
            usleep(MLDR215_FLASH_TRCV_PROG); // Trcv Prog
            flash_cmd &= ~FLASH_PROG;
            retval = target_write_u32(target, FLASH_CMD, flash_cmd);
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;
            flash_cmd &= ~(FLASH_CE);
            retval = target_write_u32(target, FLASH_CMD, flash_cmd);
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;
            usleep(MLDR215_FLASH_TRW1); // Trw
        } // while count
    } // if RESOURCE_NOT_AVAILABLE

reset_pg_and_lock:
    flash_cmd = (flash_cmd & FLASH_DELAY_MASK) | FLASH_TMEN;
    retval2 = target_write_u32(target, FLASH_CMD, flash_cmd);
    if (retval == ERROR_OK)
        retval = retval2;

    retval2 = target_write_u32(target, FLASH_KEY, 0);
    if (retval == ERROR_OK)
        retval = retval2;

free_buffer:

    /* read some bytes bytes to flush buffer in flash accelerator.
     * See errata for 1986VE1T and 1986VE3. Error 0007 */
    //if ((retval == ERROR_OK) && (!mdr215_info->mem_type)) {
    //    uint32_t tmp;
    //    target_checksum_memory(bank->target, bank->base, 64, &tmp);
    //}

    return retval;
} // mdr215_write

static int  __attribute__((unused)) mdr215_read(struct flash_bank *bank, uint8_t *buffer,
            uint32_t offset, uint32_t count)
{
    // Not implemented
    return ERROR_FLASH_OPER_UNSUPPORTED;
}

static int mdr215_probe(struct flash_bank *bank)
{
    struct mdr215_flash_bank *mdr215_info = bank->driver_priv;
    unsigned int sect_count, sect_size, i;

    sect_count = mdr215_info->sec_count;
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

    mdr215_info->probed = true;

    return ERROR_OK;
}

static int mdr215_auto_probe(struct flash_bank *bank)
{
    struct mdr215_flash_bank *mdr215_info = bank->driver_priv;
    if (mdr215_info->probed)
        return ERROR_OK;
    return mdr215_probe(bank);
}

int get_mdr215_info(struct flash_bank *bank, struct command_invocation *cmd)
{
    struct mdr215_flash_bank *mdr215_info = bank->driver_priv;
    command_print_sameline(cmd, "MDR215 - %s",
         mdr215_info->mem_type ? "info memory" : "main memory");

    return ERROR_OK;
}

int get_mdr215_info_OLDAPI(struct flash_bank *bank, char *buf, int buf_size)
{
    struct mdr215_flash_bank *mdr215_info = bank->driver_priv;
    snprintf(buf, buf_size, "MDR215 - %s",
         mdr215_info->mem_type ? "info memory" : "main memory");

    return ERROR_OK;
}

const struct flash_driver mdr215_flash = {
    .name = "mdr215",
    .usage = "flash bank <name> mdr215 <base> <size> 0 0 <target#> <type> <page_count> <sec_count>"
    "<type>: 0 for main memory, 1 for info memory",
    .flash_bank_command = mdr215_flash_bank_command,
    .erase = mdr215_erase,
    .write = mdr215_write,
    .read = default_flash_read, //mdr215_read,
    .probe = mdr215_probe,
    .auto_probe = mdr215_auto_probe,
    .erase_check = default_flash_blank_check,
    .info = __builtin_choose_expr (
        __builtin_types_compatible_p(typeof(&get_mdr215_info), typeof(mdr215_flash.info)),
        get_mdr215_info, get_mdr215_info_OLDAPI),
    .free_driver_priv = default_flash_free_driver_priv,
};
