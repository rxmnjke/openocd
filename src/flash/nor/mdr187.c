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
//#include "riscv.h"
#include <target/riscv/riscv.h>

#define MD_RST_CLK      0x40020000
#define MD_PER2_CLOCK   (MD_RST_CLK + 0x1C)
#define MD_PER2_CLOCK_EEPROM    (1 << 3)
#define MD_PER2_CLOCK_RST_CLK   (1 << 4)

#define FLASH_REG_BASE  0x40018000
#define FLASH_CMD   (FLASH_REG_BASE + 0x00)
#define FLASH_ADR   (FLASH_REG_BASE + 0x04)
#define FLASH_DI    (FLASH_REG_BASE + 0x08)
#define FLASH_DO    (FLASH_REG_BASE + 0x0C)
#define FLASH_KEY   (FLASH_REG_BASE + 0x10)

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
#define FLASH_DELAY_MASK    (7 << 3)

#define KEY     0x8AAA5551

struct mdr187_flash_bank {
    int probed;
    unsigned int mem_type;
    unsigned int page_count;
    unsigned int sec_count;
};

/* flash bank <name> mdr <base> <size> 0 0 <target#> <type> <page_count> <sec_count> */
FLASH_BANK_COMMAND_HANDLER(mdr187_flash_bank_command)
{
    struct mdr187_flash_bank *mdr187_info;

    if (CMD_ARGC < 9)
        return ERROR_COMMAND_SYNTAX_ERROR;

    mdr187_info = malloc(sizeof(struct mdr187_flash_bank));

    bank->driver_priv = mdr187_info;
    mdr187_info->probed = false;
    COMMAND_PARSE_NUMBER(uint, CMD_ARGV[6], mdr187_info->mem_type);
    COMMAND_PARSE_NUMBER(uint, CMD_ARGV[7], mdr187_info->page_count);
    COMMAND_PARSE_NUMBER(uint, CMD_ARGV[8], mdr187_info->sec_count);
    LOG_DEBUG("MDR187: flash bank");
    return ERROR_OK;
}

static int mdr187_mass_erase(struct flash_bank *bank)
{
    struct target *target = bank->target;
    struct mdr187_flash_bank *mdr187_info = bank->driver_priv;
    uint32_t flash_cmd;
    int retval;
    unsigned int i;

    retval = target_read_u32(target, FLASH_CMD, &flash_cmd);
    if (retval != ERROR_OK)
        return retval;

    for (i = 0; i < mdr187_info->sec_count; i++) {
        retval = target_write_u32(target, FLASH_ADR, i << 2);
        if (retval != ERROR_OK)
            return retval;

        flash_cmd |= FLASH_XE | FLASH_MAS1 | FLASH_ERASE;
        retval = target_write_u32(target, FLASH_CMD, flash_cmd);
        if (retval != ERROR_OK)
            return retval;
        flash_cmd |= FLASH_NVSTR;
        retval = target_write_u32(target, FLASH_CMD, flash_cmd);
        if (retval != ERROR_OK)
            return retval;
        flash_cmd &= ~FLASH_ERASE;
        retval = target_write_u32(target, FLASH_CMD, flash_cmd);
        if (retval != ERROR_OK)
            return retval;
        flash_cmd &= ~(FLASH_XE | FLASH_MAS1 | FLASH_NVSTR);
        retval = target_write_u32(target, FLASH_CMD, flash_cmd);
        if (retval != ERROR_OK)
            return retval;
    }

    return retval;
}

static int mdr187_erase(struct flash_bank *bank, unsigned int first, unsigned int last)
{
    struct target *target = bank->target;
    struct mdr187_flash_bank *mdr187_info = bank->driver_priv;
    int retval, retval2;
    unsigned int j;
    uint32_t flash_cmd, cur_per_clock;

    LOG_INFO ("MDR187: ERASE pages from 0x%"PRIx32" to 0x%"PRIx32"", first, last);

    if (bank->target->state != TARGET_HALTED) {
        LOG_ERROR("Target not halted");
        return ERROR_TARGET_NOT_HALTED;
    }

    retval = target_read_u32(target, MD_PER2_CLOCK, &cur_per_clock);
    if (retval != ERROR_OK)
        return retval;

    if (!(cur_per_clock & 0x10)) {
        LOG_ERROR("Target needs reset before flash operations");
        return ERROR_FLASH_OPERATION_FAILED;
    }

    retval = target_write_u32(target, MD_PER2_CLOCK, cur_per_clock | MD_PER2_CLOCK_EEPROM);
    if (retval != ERROR_OK)
        return retval;

    retval = target_write_u32(target, FLASH_KEY, KEY);
    if (retval != ERROR_OK)
        return retval;

    retval = target_read_u32(target, FLASH_CMD, &flash_cmd);
    if (retval != ERROR_OK)
        goto reset_pg_and_lock;

    /* Switch on register access */
    flash_cmd = (flash_cmd & FLASH_DELAY_MASK) | FLASH_CON | FLASH_TMR;
    if (mdr187_info->mem_type)
        flash_cmd |= FLASH_IFREN;
    retval = target_write_u32(target, FLASH_CMD, flash_cmd);
    if (retval != ERROR_OK)
        goto reset_pg_and_lock;

    if ((first == 0) && (last == (bank->num_sectors - 1)) &&
        !mdr187_info->mem_type) {
        retval = mdr187_mass_erase(bank);
        goto reset_pg_and_lock;
    }

    unsigned int page_size = bank->size / mdr187_info->page_count;
    LOG_DEBUG("MDR187: page_size 0x%"PRIx32" page_count 0x%"PRIx32" sec_count 0x%"PRIx32"\n", page_size, mdr187_info->page_count, mdr187_info->sec_count);

    for (unsigned int i = first; i <= last; i++) {
        for (j = 0; j < mdr187_info->sec_count; j++) {
            retval = target_write_u32(target, FLASH_ADR, (i * page_size) | (j << 2));
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;

            flash_cmd |= FLASH_XE | FLASH_ERASE;
            retval = target_write_u32(target, FLASH_CMD, flash_cmd);
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;
            flash_cmd |= FLASH_NVSTR;
            retval = target_write_u32(target, FLASH_CMD, flash_cmd);
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;
            flash_cmd &= ~FLASH_ERASE;
            retval = target_write_u32(target, FLASH_CMD, flash_cmd);
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;
            flash_cmd &= ~(FLASH_XE | FLASH_NVSTR);
            retval = target_write_u32(target, FLASH_CMD, flash_cmd);
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;
        }
        bank->sectors[i].is_erased = 1;
    }

reset_pg_and_lock:
    flash_cmd &= FLASH_DELAY_MASK;
    retval2 = target_write_u32(target, FLASH_CMD, flash_cmd);
    if (retval == ERROR_OK)
        retval = retval2;

    retval2 = target_write_u32(target, FLASH_KEY, 0x0);
    if (retval == ERROR_OK)
        retval = retval2;

    return retval;
}

static int mdr187_write_block(struct flash_bank *bank, const uint8_t *buffer,
        uint32_t offset, uint32_t count)
{
    struct target *target = bank->target;
    struct mdr187_flash_bank *mdr187_info = bank->driver_priv;
    unsigned int page_size = bank->size / mdr187_info->page_count;
    uint32_t buffer_size = page_size;
    struct working_area *write_algorithm;
    struct working_area *source;
    uint32_t address = bank->base + offset;
    struct reg_param reg_params[5];
    int retval = ERROR_OK;

    /* see ... for src */
    static const uint8_t mdr187_flash_write_code[] = {
                                0xB7, 0x87, 0x01, 0x40, 0x23, 0xA8, 0xD7, 0x00,
        0xB7, 0x56, 0xAA, 0x8A, 0x93, 0x86, 0x16, 0x55, 0x23, 0xA8, 0xD7, 0x00,
        0x83, 0xAF, 0x07, 0x00, 0x37, 0x48, 0x00, 0x00, 0x93, 0xFF, 0x8F, 0x03,
        0x63, 0x18, 0x05, 0x12, 0x13, 0x08, 0x18, 0x00, 0x33, 0xE8, 0x0F, 0x01,
        0xB7, 0x37, 0x00, 0x00, 0x37, 0x1E, 0x00, 0x00, 0x37, 0x23, 0x00, 0x00,
        0x13, 0x85, 0x07, 0x04, 0x13, 0x9F, 0x25, 0x00, 0x93, 0x87, 0x07, 0x0C,
        0xB7, 0x86, 0x01, 0x40, 0x13, 0x0E, 0x0E, 0x04, 0x13, 0x03, 0x03, 0x04,
        0x23, 0xA0, 0x06, 0x01, 0x33, 0x6E, 0xC8, 0x01, 0x33, 0x65, 0xA8, 0x00,
        0xB3, 0x68, 0xF8, 0x00, 0x33, 0x63, 0x68, 0x00, 0xB3, 0x0E, 0xE7, 0x01,
        0x93, 0x05, 0x07, 0x00, 0xB7, 0x86, 0x01, 0x40, 0xB3, 0x87, 0xE5, 0x40,
        0x23, 0xA2, 0xB6, 0x00, 0xB3, 0x07, 0xF6, 0x00, 0x83, 0xA7, 0x07, 0x00,
        0x23, 0xA4, 0xF6, 0x00, 0x23, 0xA0, 0xC6, 0x01, 0xF3, 0x26, 0x00, 0xB0,
        0x93, 0x86, 0x86, 0x02, 0xF3, 0x27, 0x00, 0xB0, 0xB3, 0x87, 0xD7, 0x40,
        0xE3, 0xCC, 0x07, 0xFE, 0xB7, 0x87, 0x01, 0x40, 0x23, 0xA0, 0xA7, 0x00,
        0xF3, 0x26, 0x00, 0xB0, 0x93, 0x86, 0x06, 0x05, 0xF3, 0x27, 0x00, 0xB0,
        0xB3, 0x87, 0xD7, 0x40, 0xE3, 0xCC, 0x07, 0xFE, 0xB7, 0x87, 0x01, 0x40,
        0x23, 0xA0, 0x17, 0x01, 0xF3, 0x26, 0x00, 0xB0, 0x93, 0x86, 0x06, 0x14,
        0xF3, 0x27, 0x00, 0xB0, 0xB3, 0x87, 0xD7, 0x40, 0xE3, 0xCC, 0x07, 0xFE,
        0xB7, 0x87, 0x01, 0x40, 0x23, 0xA0, 0xA7, 0x00, 0x23, 0xA0, 0x67, 0x00,
        0xF3, 0x26, 0x00, 0xB0, 0x93, 0x86, 0x86, 0x02, 0xF3, 0x27, 0x00, 0xB0,
        0xB3, 0x87, 0xD7, 0x40, 0xE3, 0xCC, 0x07, 0xFE, 0xB7, 0x87, 0x01, 0x40,
        0x23, 0xA0, 0x07, 0x01, 0xF3, 0x26, 0x00, 0xB0, 0x93, 0x86, 0x06, 0x05,
        0xF3, 0x27, 0x00, 0xB0, 0xB3, 0x87, 0xD7, 0x40, 0xE3, 0xCC, 0x07, 0xFE,
        0x93, 0x85, 0x45, 0x00, 0xE3, 0x9C, 0xBE, 0xF4, 0xB7, 0x87, 0x01, 0x40,
        0x23, 0xA0, 0xF7, 0x01, 0x23, 0xA8, 0x07, 0x00, 0x13, 0x07, 0x47, 0x00,
        0x93, 0x07, 0xA0, 0x02, 0x13, 0x85, 0x07, 0x00, 0x13, 0x0F, 0xCF, 0xFF,
        0x33, 0x07, 0xEF, 0x00, 0x13, 0x07, 0x07, 0x00,
        0x73, 0x00, 0x10, 0x00 //73001000        ebreak
    };

    /* flash write code */
    LOG_DEBUG("MDR187: request  0x%"PRIx32" bytes\n", (unsigned int)sizeof(mdr187_flash_write_code));
    if (target_alloc_working_area(target, sizeof(mdr187_flash_write_code),
            &write_algorithm) != ERROR_OK) {
        LOG_WARNING("no working area available, can't do block memory writes");
        return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
    }

    retval = target_write_buffer(target, write_algorithm->address,
            sizeof(mdr187_flash_write_code), mdr187_flash_write_code);
    if (retval != ERROR_OK)
        return retval;

    /* memory buffer */
    LOG_DEBUG("MDR187: request  0x%"PRIx32" bytes\n", buffer_size);
    while (target_alloc_working_area_try(target, buffer_size, &source) != ERROR_OK) {
//      buffer_size /= 2;
//      buffer_size &= ~3UL; /* Make sure it's 4 byte aligned */
//      if (buffer_size <= 256) {
            /* we already allocated the writing code, but failed to get a
             * buffer, free the algorithm */
            target_free_working_area(target, write_algorithm);

            LOG_WARNING("no large enough working area available, can't do block memory writes");
            return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
//      }
    }
    while (count>0)    {
        unsigned int bytes_to_write = buffer_size<count?buffer_size:count;//min(buffer_size, count);
    // a0=x10 s10=x26
        init_reg_param(&reg_params[0], "a0", 32, PARAM_IN_OUT); /* flash base (in), status (out) */
        init_reg_param(&reg_params[1], "a1", 32, PARAM_OUT);    /* word_count (32bit) */
        init_reg_param(&reg_params[2], "a2", 32, PARAM_OUT);    /* buffer start */
        init_reg_param(&reg_params[3], "a3", 32, PARAM_OUT);    /* buffer end */
        init_reg_param(&reg_params[4], "a4", 32, PARAM_IN_OUT); /* target address */

        retval = target_write_buffer(target, source->address, bytes_to_write, buffer);
        if (retval != ERROR_OK)
            goto free_buffer;

        buf_set_u32(reg_params[0].value, 0, 32, mdr187_info->mem_type);
        buf_set_u32(reg_params[1].value, 0, 32, bytes_to_write/4);
        buf_set_u32(reg_params[2].value, 0, 32, source->address);
        buf_set_u32(reg_params[3].value, 0, 32, source->address + source->size);
        buf_set_u32(reg_params[4].value, 0, 32, address);

        LOG_DEBUG("MDR187: mem_type       0x%"PRIx32"\n", buf_get_u32(reg_params[0].value, 0, 32));
        LOG_DEBUG("MDR187: word_count     0x%"PRIx32"\n", buf_get_u32(reg_params[1].value, 0, 32));
        LOG_DEBUG("MDR187: start          0x%"PRIx32"\n", buf_get_u32(reg_params[2].value, 0, 32));
        LOG_DEBUG("MDR187: end            0x%"PRIx32"\n", buf_get_u32(reg_params[3].value, 0, 32));
        LOG_DEBUG("MDR187: address        0x%"PRIx32"\n", buf_get_u32(reg_params[4].value, 0, 32));

    //int target_run_algorithm(struct target *target,
    //      int num_mem_params, struct mem_param *mem_params,
    //      int num_reg_params, struct reg_param *reg_param,
    //      uint32_t entry_point, uint32_t exit_point,
    //      int timeout_ms, void *arch_info)

        retval = target_run_algorithm(target,
                0, NULL,
                5, reg_params,
                write_algorithm->address, 0,
                1000, NULL);

        LOG_DEBUG("MDR187: status      0x%"PRIx32"\n", buf_get_u32(reg_params[0].value, 0, 32));
        LOG_DEBUG("MDR187: word_count  0x%"PRIx32"\n", buf_get_u32(reg_params[1].value, 0, 32));
        LOG_DEBUG("MDR187: start       0x%"PRIx32"\n", buf_get_u32(reg_params[2].value, 0, 32));
        LOG_DEBUG("MDR187: end         0x%"PRIx32"\n", buf_get_u32(reg_params[3].value, 0, 32));
        LOG_DEBUG("MDR187: address     0x%"PRIx32"\n", buf_get_u32(reg_params[4].value, 0, 32));

        if (retval == ERROR_FLASH_OPERATION_FAILED) {
            LOG_ERROR("flash write failed at address 0x%"PRIx32,
                    buf_get_u32(reg_params[4].value, 0, 32));
                    break;
            }
        address += bytes_to_write;
        count -= bytes_to_write;
        buffer  += bytes_to_write;
    } // while
free_buffer:
    target_free_working_area(target, source);
    target_free_working_area(target, write_algorithm);

    destroy_reg_param(&reg_params[0]);
    destroy_reg_param(&reg_params[1]);
    destroy_reg_param(&reg_params[2]);
    destroy_reg_param(&reg_params[3]);
    destroy_reg_param(&reg_params[4]);

    return retval;
}

static int mdr187_write(struct flash_bank *bank, const uint8_t *buffer,
        uint32_t offset, uint32_t count)
{
    struct target *target = bank->target;
    struct mdr187_flash_bank *mdr187_info = bank->driver_priv;
    uint8_t *new_buffer = NULL;

    if (bank->target->state != TARGET_HALTED) {
        LOG_ERROR("Target not halted");
        return ERROR_TARGET_NOT_HALTED;
    }

    if (offset & 0x3) {
        LOG_ERROR("offset 0x%" PRIx32 " breaks required 4-byte alignment", offset);
        return ERROR_FLASH_DST_BREAKS_ALIGNMENT;
    }

    /* If there's an odd number of bytes, the data has to be padded. Duplicate
     * the buffer and use the normal code path with a single block write since
     * it's probably cheaper than to special case the last odd write using
     * discrete accesses. */
    int rem = count % 4;
    if (rem) {
        new_buffer = malloc(count + rem);
        if (new_buffer == NULL) {
            LOG_ERROR("odd number of bytes to write and no memory for padding buffer");
            return ERROR_FAIL;
        }
        LOG_INFO("odd number of bytes to write, padding with 0xff");
        buffer = memcpy(new_buffer, buffer, count);
        while (rem--)
            new_buffer[count++] = 0xff;
    }

    uint32_t flash_cmd, cur_per_clock;
    int retval, retval2;

    retval = target_read_u32(target, MD_PER2_CLOCK, &cur_per_clock);
    if (retval != ERROR_OK)
        goto free_buffer;

    if (!(cur_per_clock & MD_PER2_CLOCK_RST_CLK)) {
        /* Something's very wrong if the RST_CLK module is not clocked */
        LOG_ERROR("Target needs reset before flash operations");
        retval = ERROR_FLASH_OPERATION_FAILED;
        goto free_buffer;
    }

    retval = target_write_u32(target, MD_PER2_CLOCK, cur_per_clock | MD_PER2_CLOCK_EEPROM);
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
    if (mdr187_info->mem_type)
        flash_cmd |= FLASH_IFREN;
    retval = target_write_u32(target, FLASH_CMD, flash_cmd);
    if (retval != ERROR_OK)
        goto reset_pg_and_lock;

    unsigned int page_size = bank->size / mdr187_info->page_count;
    LOG_INFO ("MDR187: PROGRAM 0x%"PRIx32" bytes at offset 0x%"PRIx32"", count, offset);
    LOG_DEBUG("MDR187: bank_size 0x%"PRIx32" page_size 0x%"PRIx32" page_count 0x%"PRIx32" sec_count 0x%"PRIx32"\n", bank->size, page_size, mdr187_info->page_count, mdr187_info->sec_count);
    /* try using block write */
    retval = mdr187_write_block(bank, buffer, offset, count);

    if (retval == ERROR_TARGET_RESOURCE_NOT_AVAILABLE) {
        /* if block write failed (no sufficient working area),
         * we use normal (slow) single halfword accesses */
        LOG_WARNING("Can't use block writes, falling back to single memory accesses");

//  if (1) {

        while (count > 0) {
            unsigned int i, j;
            unsigned int page_mask = page_size - 1;
            unsigned int cur_page = offset & ~page_mask;
            unsigned int bytes_to_write = cur_page + page_size - offset;
            if (count < bytes_to_write)
                bytes_to_write = count;

            LOG_DEBUG("Selecting next page: %08x", cur_page);

            for (i = 0; i < mdr187_info->sec_count; i++) {
                retval = target_write_u32(target, FLASH_ADR, offset + i*4);
                if (retval != ERROR_OK)
                    goto reset_pg_and_lock;
                LOG_DEBUG("Selecting page/sector: %08x", offset + i*4);

                flash_cmd |= FLASH_XE | FLASH_PROG;
                retval = target_write_u32(target, FLASH_CMD, flash_cmd);
                if (retval != ERROR_OK)
                    goto reset_pg_and_lock;

                flash_cmd |= FLASH_NVSTR;
                retval = target_write_u32(target, FLASH_CMD, flash_cmd);
                if (retval != ERROR_OK)
                    goto reset_pg_and_lock;

                for (j = 0;
                     (((offset + j + i*4) & ~page_mask) == cur_page) &&
                         (j + i*4 < count);
                     j += mdr187_info->sec_count*4) {
                    uint32_t value;
                    memcpy(&value, buffer + j + i*4, sizeof(uint32_t));
                    retval = target_write_u32(target, FLASH_DI, value);
                    if (retval != ERROR_OK)
                        goto reset_pg_and_lock;
                    //LOG_DEBUG("Writing to addr %08x", offset + j + i*4);
                    retval = target_write_u32(target, FLASH_ADR, offset + j + i*4);
                    if (retval != ERROR_OK)
                        goto reset_pg_and_lock;

                    flash_cmd |= FLASH_YE;
                    retval = target_write_u32(target, FLASH_CMD, flash_cmd);
                    if (retval != ERROR_OK)
                        goto reset_pg_and_lock;
                    flash_cmd &= ~FLASH_YE;
                    retval = target_write_u32(target, FLASH_CMD, flash_cmd);
                    if (retval != ERROR_OK)
                        goto reset_pg_and_lock;
                }
                flash_cmd &= ~FLASH_NVSTR;
                retval = target_write_u32(target, FLASH_CMD, flash_cmd);
                if (retval != ERROR_OK)
                    goto reset_pg_and_lock;

                flash_cmd &= ~(FLASH_XE | FLASH_PROG);
                retval = target_write_u32(target, FLASH_CMD, flash_cmd);
                if (retval != ERROR_OK)
                    goto reset_pg_and_lock;
            }

            buffer += bytes_to_write;
            offset += bytes_to_write;
            count -= bytes_to_write;
        }
    }

reset_pg_and_lock:
    flash_cmd &= FLASH_DELAY_MASK;
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
    if ((retval == ERROR_OK) && (!mdr187_info->mem_type)) {
        uint32_t tmp;
        target_checksum_memory(bank->target, bank->base, 64, &tmp);
    }

    return retval;
}

static int mdr187_read(struct flash_bank *bank, uint8_t *buffer,
            uint32_t offset, uint32_t count)
{
    struct target *target = bank->target;
    struct mdr187_flash_bank *mdr187_info = bank->driver_priv;
    int retval, retval2;

    LOG_DEBUG("MDR187: offset 0x%"PRIx32" count 0x%"PRIx32"\n", offset, count);

    if (!mdr187_info->mem_type)
        return default_flash_read(bank, buffer, offset, count);

    if (bank->target->state != TARGET_HALTED) {
        LOG_ERROR("Target not halted");
        return ERROR_TARGET_NOT_HALTED;
    }

    if (offset & 0x3) {
        LOG_ERROR("offset 0x%" PRIx32 " breaks required 4-byte alignment", offset);
        return ERROR_FLASH_DST_BREAKS_ALIGNMENT;
    }

    if (count & 0x3) {
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

    retval = target_write_u32(target, MD_PER2_CLOCK, cur_per_clock | MD_PER2_CLOCK_EEPROM);
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

static int mdr187_probe(struct flash_bank *bank)
{
    struct mdr187_flash_bank *mdr187_info = bank->driver_priv;
    unsigned int page_count, page_size, i;

    page_count = mdr187_info->page_count;
    page_size = bank->size / page_count;

    //if (bank->sectors) {
        free(bank->sectors);
    //  bank->sectors = NULL;
    //}

    bank->num_sectors = page_count;
    bank->sectors = malloc(sizeof(struct flash_sector) * page_count);

    for (i = 0; i < page_count; i++) {
        bank->sectors[i].offset = i * page_size;
        bank->sectors[i].size = page_size;
        bank->sectors[i].is_erased = -1;
        bank->sectors[i].is_protected = 0;
    }

    mdr187_info->probed = true;

    return ERROR_OK;
}

static int mdr187_auto_probe(struct flash_bank *bank)
{
    struct mdr187_flash_bank *mdr187_info = bank->driver_priv;
    if (mdr187_info->probed)
        return ERROR_OK;
    return mdr187_probe(bank);
}

int get_mdr187_info(struct flash_bank *bank, struct command_invocation *cmd)
{
    struct mdr187_flash_bank *mdr187_info = bank->driver_priv;
    command_print_sameline(cmd, "MDR187 - %s",
         mdr187_info->mem_type ? "info memory" : "main memory");

    return ERROR_OK;
}

int get_mdr187_info_OLDAPI(struct flash_bank *bank, char *buf, int buf_size)
{
    struct mdr187_flash_bank *mdr187_info = bank->driver_priv;
    snprintf(buf, buf_size, "MDR187 - %s",
         mdr187_info->mem_type ? "info memory" : "main memory");

    return ERROR_OK;
}

const struct flash_driver mdr187_flash = {
    .name = "mdr187",
    .usage = "flash bank <name> mdr187 <base> <size> 0 0 <target#> <type> <page_count> <sec_count>\n"
    "<type>: 0 for main memory, 1 for info memory",
    .flash_bank_command = mdr187_flash_bank_command,
    .erase = mdr187_erase,
    .write = mdr187_write,
    .read = mdr187_read,
    .probe = mdr187_probe,
    .auto_probe = mdr187_auto_probe,
    .erase_check = default_flash_blank_check,
    .info = __builtin_choose_expr (__builtin_types_compatible_p(typeof(&get_mdr187_info), typeof(mdr187_flash.info)), get_mdr187_info, get_mdr187_info_OLDAPI),
    .free_driver_priv = default_flash_free_driver_priv,
};
