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
#include <target/armv7m.h>
//#include <target/target_type.h>
#include <stdint.h>

#define FLASH_REG_BASE  0x40006000
#define FLASH_KEY     (FLASH_REG_BASE + 0x00)
#define FLASH_CMD     (FLASH_REG_BASE + 0x04)
#define FLASH_ADR     (FLASH_REG_BASE + 0x08)
#define FLASH_WDATA0  (FLASH_REG_BASE + 0x18)
#define FLASH_WDATA1  (FLASH_REG_BASE + 0x14)
#define FLASH_WDATA2  (FLASH_REG_BASE + 0x10)
#define FLASH_WDATA3  (FLASH_REG_BASE + 0x0C)
#define FLASH_WECC0   (FLASH_REG_BASE + 0x1C)
#define FLASH_WECC1   (FLASH_REG_BASE + 0x20)
#define FLASH_RDATA0  (FLASH_REG_BASE + 0x30)
#define FLASH_RDATA1  (FLASH_REG_BASE + 0x2C)
#define FLASH_RDATA2  (FLASH_REG_BASE + 0x28)
#define FLASH_RDATA3  (FLASH_REG_BASE + 0x24)
#define FLASH_RECC0   (FLASH_REG_BASE + 0x34)
#define FLASH_RECC1   (FLASH_REG_BASE + 0x38)
#define FLASH_BLOCK   (FLASH_REG_BASE + 0x4C)


#define FLASH_CNTR_FLASH5_CR_Pos          (31UL)                    /*!< FLASH5_CR (Bit 31)                  */
#define FLASH_CNTR_FLASH5_CR_Msk          (0x80000000UL)            /*!< FLASH5_CR (Bitfield-Mask: 0x01)     */
#define FLASH_CNTR_FLASH4_CR_Pos          (30UL)                    /*!< FLASH4_CR (Bit 30)                  */
#define FLASH_CNTR_FLASH4_CR_Msk          (0x40000000UL)            /*!< FLASH4_CR (Bitfield-Mask: 0x01)     */
#define FLASH_CNTR_FLASH3_CR_Pos          (29UL)                    /*!< FLASH3_CR (Bit 29)                  */
#define FLASH_CNTR_FLASH3_CR_Msk          (0x20000000UL)            /*!< FLASH3_CR (Bitfield-Mask: 0x01)     */
#define FLASH_CNTR_FLASH2_CR_Pos          (28UL)                    /*!< FLASH2_CR (Bit 28)                  */
#define FLASH_CNTR_FLASH2_CR_Msk          (0x10000000UL)            /*!< FLASH2_CR (Bitfield-Mask: 0x01)     */
#define FLASH_CNTR_FLASH1_CR_Pos          (27UL)                    /*!< FLASH1_CR (Bit 27)                  */
#define FLASH_CNTR_FLASH1_CR_Msk          (0x8000000UL)             /*!< FLASH1_CR (Bitfield-Mask: 0x01)     */
#define FLASH_CNTR_FLASH0_CR_Pos          (26UL)                    /*!< FLASH0_CR (Bit 26)                  */
#define FLASH_CNTR_FLASH0_CR_Msk          (0x4000000UL)             /*!< FLASH0_CR (Bitfield-Mask: 0x01)     */
#define FLASH_CNTR_FLASH50_CR_Msk         (0xFC000000UL)            /*!< FLASH0_CR (Bitfield-Mask: 0x01)     */
#define FLASH_CNTR_FLASH54_CR_Msk         (0xC0000000UL)            /*!< FLASH0_CR (Bitfield-Mask: 0x01)     */
#define FLASH_CNTR_FLASH30_CR_Msk         (0x3C000000UL)            /*!< FLASH0_CR (Bitfield-Mask: 0x01)     */
#define FLASH_CNTR_breakthrough_Pos       (24UL)                    /*!< breakthrough (Bit 24)               */
#define FLASH_CNTR_breakthrough_Msk       (0x3000000UL)             /*!< breakthrough (Bitfield-Mask: 0x03)  */
#define FLASH_CNTR_ERASE_DONE_Pos         (23UL)                    /*!< ERASE_DONE (Bit 23)                 */
#define FLASH_CNTR_ERASE_DONE_Msk         (0x800000UL)              /*!< ERASE_DONE (Bitfield-Mask: 0x01)    */
#define FLASH_CNTR_rdata_ready_Pos        (22UL)                    /*!< rdata_ready (Bit 22)                */
#define FLASH_CNTR_rdata_ready_Msk        (0x400000UL)              /*!< rdata_ready (Bitfield-Mask: 0x01)   */
#define FLASH_CNTR_IFREN_Pos              (21UL)                    /*!< IFREN (Bit 21)                      */
#define FLASH_CNTR_IFREN_Msk              (0x200000UL)              /*!< IFREN (Bitfield-Mask: 0x01)         */
#define FLASH_CNTR_NVSTR_Pos              (20UL)                    /*!< NVSTR (Bit 20)                      */
#define FLASH_CNTR_NVSTR_Msk              (0x100000UL)              /*!< NVSTR (Bitfield-Mask: 0x01)         */
#define FLASH_CNTR_PROG_Pos               (19UL)                    /*!< PROG (Bit 19)                       */
#define FLASH_CNTR_PROG_Msk               (0x80000UL)               /*!< PROG (Bitfield-Mask: 0x01)          */
#define FLASH_CNTR_MAS1_Pos               (18UL)                    /*!< MAS1 (Bit 18)                       */
#define FLASH_CNTR_MAS1_Msk               (0x40000UL)               /*!< MAS1 (Bitfield-Mask: 0x01)          */
#define FLASH_CNTR_ERASE_Pos              (17UL)                    /*!< ERASE (Bit 17)                      */
#define FLASH_CNTR_ERASE_Msk              (0x20000UL)               /*!< ERASE (Bitfield-Mask: 0x01)         */
#define FLASH_CNTR_YE_Pos                 (16UL)                    /*!< YE (Bit 16)                         */
#define FLASH_CNTR_YE_Msk                 (0x10000UL)               /*!< YE (Bitfield-Mask: 0x01)            */
#define FLASH_CNTR_XE_Pos                 (15UL)                    /*!< XE (Bit 15)                         */
#define FLASH_CNTR_XE_Msk                 (0x8000UL)                /*!< XE (Bitfield-Mask: 0x01)            */
#define FLASH_CNTR_SE_Pos                 (9UL)                     /*!< SE (Bit 9)                          */
#define FLASH_CNTR_SE50_Msk               (0x7e00UL)                /*!< SE (Bitfield-Mask: 0x3f)            */
#define FLASH_CNTR_SE30_Msk               (0x1e00UL)                /*!< SE (Bitfield-Mask: 0x3f)            */
#define FLASH_CNTR_TMR_Pos                (8UL)                     /*!< TMR (Bit 8)                         */
#define FLASH_CNTR_TMR_Msk                (0x100UL)                 /*!< TMR (Bitfield-Mask: 0x01)           */
#define FLASH_CNTR_ERASE_START_Pos        (7UL)                     /*!< ERASE_START (Bit 7)                 */
#define FLASH_CNTR_ERASE_START_Msk        (0x80UL)                  /*!< ERASE_START (Bitfield-Mask: 0x01)   */
#define FLASH_CNTR_BRKTHRU_DONE_Pos       (6UL)                     /*!< BRKTHRU_DONE (Bit 6)                */
#define FLASH_CNTR_BRKTHRU_DONE_Msk       (0x40UL)                  /*!< BRKTHRU_DONE (Bitfield-Mask: 0x01)  */
#define FLASH_CNTR_BRKTHRU_START_Pos      (5UL)                     /*!< BRKTHRU_START (Bit 5)               */
#define FLASH_CNTR_BRKTHRU_START_Msk      (0x20UL)                  /*!< BRKTHRU_START (Bitfield-Mask: 0x01) */
#define FLASH_CNTR_MODE_Pos               (4UL)                     /*!< MODE (Bit 4)                        */
#define FLASH_CNTR_MODE_Msk               (0x10UL)                  /*!< MODE (Bitfield-Mask: 0x01)          */
#define FLASH_CNTR_WAIT_Pos               (0UL)                     /*!< WAIT (Bit 0)                        */
#define FLASH_CNTR_WAIT_Msk               (0xfUL)                   /*!< WAIT (Bitfield-Mask: 0x0f)          */

#define KEY     0x8555AAA1

#define M32(adr) (*((uint32_t *) (adr)))

#define UPD(flash_cmd)                                            \
    do {                                                          \
        retval = target_write_u32(target, FLASH_CMD, flash_cmd);  \
        if (retval != ERROR_OK)                                   \
            goto reset_pg_and_lock;                               \
    } while (0);

#define UNUSED_PARAMETER(x) (void)(x)

struct mdr149_flash_bank {
    int probed;
    uint32_t mem_type;
    uint32_t page_count;
    uint32_t lock_code;
};

/* flash bank <name> mdr149 <base> <size> 0 0 <target#> <type> <page_count> <lock_code> */
FLASH_BANK_COMMAND_HANDLER(mdr149_flash_bank_command)
{
    struct mdr149_flash_bank *mdr149_info;

    if (CMD_ARGC < 9)
        return ERROR_COMMAND_SYNTAX_ERROR;

    mdr149_info = malloc(sizeof(struct mdr149_flash_bank));

    bank->driver_priv = mdr149_info;
    mdr149_info->probed = false;
    COMMAND_PARSE_NUMBER(uint, CMD_ARGV[6], mdr149_info->mem_type);
    COMMAND_PARSE_NUMBER(uint, CMD_ARGV[7], mdr149_info->page_count);
    COMMAND_PARSE_NUMBER(uint, CMD_ARGV[8], mdr149_info->lock_code);
    LOG_DEBUG("MDR149: flash bank");
    return ERROR_OK;
}

uint8_t ecc(uint32_t addr, uint32_t data)
{
    uint32_t r = 0;
    r ^= (((addr >> 0) & 0x01010101) * 0xFF) & 0x91C86432;
    r ^= (((addr >> 1) & 0x01010101) * 0xFF) & 0xA1D06834;
    r ^= (((addr >> 2) & 0x01010101) * 0xFF) & 0xC1E07038;
    r ^= (((addr >> 3) & 0x01010101) * 0xFF) & 0x9E4FA7D3;
    r ^= (((addr >> 4) & 0x01010101) * 0xFF) & 0xA251A854;
    r ^= (((addr >> 5) & 0x01010101) * 0xFF) & 0xC261B058;
    r ^= (((addr >> 6) & 0x01010101) * 0xFF) & 0xC4623198;
    r ^= (((addr >> 7) & 0x01010101) * 0xFF) & 0xA4522994;
    r ^= (((data >> 0) & 0x01010101) * 0xFF) & 0x198C4623;
    r ^= (((data >> 1) & 0x01010101) * 0xFF) & 0x1A0D8643;
    r ^= (((data >> 2) & 0x01010101) * 0xFF) & 0x1C0E0783;
    r ^= (((data >> 3) & 0x01010101) * 0xFF) & 0xE9F47A3D;
    r ^= (((data >> 4) & 0x01010101) * 0xFF) & 0x2A158A45;
    r ^= (((data >> 5) & 0x01010101) * 0xFF) & 0x2C160B85;
    r ^= (((data >> 6) & 0x01010101) * 0xFF) & 0x4C261389;
    r ^= (((data >> 7) & 0x01010101) * 0xFF) & 0x4A259249;
    r ^= r >> 16;
    r ^= r >> 8;
    return r & 0xFF;
}
const uint32_t rCrc32Tab[] __attribute__((aligned(8))) = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
    0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
    0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
    0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
    0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
    0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
    0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
    0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
    0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
    0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
    0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
    0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
    0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
    0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
    0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
    0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
    0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
    0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
    0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
    0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
    0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
    0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94, 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

/* see contrib/loaders/flash/??? for src */
static const uint8_t mdr149_code[] = {
0x40, 0xBA, 0x70, 0x47, 0xC0, 0xBA, 0x70, 0x47, 0x4F, 0xEA, 0x30, 0x00,
0x70, 0x47, 0x00, 0x00, 0x30, 0xB5, 0x00, 0xF0, 0x01, 0x32, 0xC2, 0xEB,
0x02, 0x24, 0xFE, 0x4A, 0xFE, 0x4D, 0x14, 0x40, 0x4F, 0xF0, 0x01, 0x32,
0x02, 0xEA, 0x50, 0x03, 0xC3, 0xEB, 0x03, 0x23, 0x2B, 0x40, 0x63, 0x40,
0x02, 0xEA, 0x90, 0x04, 0xF9, 0x4D, 0xC4, 0xEB, 0x04, 0x24, 0x2C, 0x40,
0x5C, 0x40, 0x02, 0xEA, 0xD0, 0x03, 0xF7, 0x4D, 0xC3, 0xEB, 0x03, 0x23,
0x2B, 0x40, 0x63, 0x40, 0x02, 0xEA, 0x10, 0x14, 0xF4, 0x4D, 0xC4, 0xEB,
0x04, 0x24, 0x2C, 0x40, 0x5C, 0x40, 0x02, 0xEA, 0x50, 0x13, 0xF2, 0x4D,
0xC3, 0xEB, 0x03, 0x23, 0x2B, 0x40, 0x63, 0x40, 0x02, 0xEA, 0x90, 0x14,
0xEF, 0x4D, 0xC4, 0xEB, 0x04, 0x24, 0x2C, 0x40, 0x5C, 0x40, 0x02, 0xEA,
0xD0, 0x10, 0xED, 0x4B, 0xC0, 0xEB, 0x00, 0x20, 0x18, 0x40, 0x60, 0x40,
0x01, 0xF0, 0x01, 0x33, 0xC3, 0xEB, 0x03, 0x23, 0x4F, 0xEA, 0x35, 0x34,
0x23, 0x40, 0x43, 0x40, 0x02, 0xEA, 0x51, 0x00, 0xE6, 0x4C, 0xC0, 0xEB,
0x00, 0x20, 0x20, 0x40, 0x58, 0x40, 0x02, 0xEA, 0x91, 0x03, 0xE4, 0x4C,
0xC3, 0xEB, 0x03, 0x23, 0x23, 0x40, 0x43, 0x40, 0x02, 0xEA, 0xD1, 0x00,
0xE1, 0x4C, 0xC0, 0xEB, 0x00, 0x20, 0x20, 0x40, 0x58, 0x40, 0x02, 0xEA,
0x11, 0x13, 0xC3, 0xEB, 0x03, 0x24, 0xDE, 0x4B, 0x1C, 0x40, 0x44, 0x40,
0x02, 0xEA, 0x51, 0x10, 0xC0, 0xEB, 0x00, 0x23, 0xDB, 0x48, 0x03, 0x40,
0x63, 0x40, 0x02, 0xEA, 0x91, 0x10, 0xDA, 0x4C, 0xC0, 0xEB, 0x00, 0x20,
0x20, 0x40, 0x02, 0xEA, 0xD1, 0x11, 0xD8, 0x4A, 0xC1, 0xEB, 0x01, 0x21,
0x58, 0x40, 0x11, 0x40, 0x41, 0x40, 0x81, 0xEA, 0x11, 0x40, 0x80, 0xEA,
0x10, 0x20, 0xC0, 0xB2, 0x30, 0xBD, 0xC1, 0x00, 0x4F, 0xF0, 0xE0, 0x20,
0x41, 0x61, 0x05, 0x21, 0x01, 0x61, 0x00, 0x21, 0x41, 0x61, 0x02, 0x69,
0xD2, 0x03, 0xFC, 0xD5, 0x01, 0x61, 0x70, 0x47, 0x2D, 0xE9, 0xF0, 0x4D,
0xCC, 0x4E, 0x05, 0x46, 0x1F, 0x31, 0xCA, 0x48, 0x9B, 0x46, 0x14, 0x46,
0x21, 0xF0, 0x1F, 0x07, 0x30, 0x60, 0xD9, 0xE0, 0x40, 0xF0, 0x10, 0x00,
0x70, 0x60, 0x70, 0x68, 0x40, 0xF4, 0x80, 0x70, 0x70, 0x60, 0x70, 0x68,
0x20, 0xF4, 0xFC, 0x40, 0x70, 0x60, 0x70, 0x68, 0x20, 0xF4, 0xF0, 0x10,
0x70, 0x60, 0x71, 0x68, 0x41, 0xEA, 0x4B, 0x50, 0x70, 0x60, 0x70, 0x68,
0x40, 0xF0, 0x70, 0x50, 0x70, 0x60, 0x20, 0x68, 0xB0, 0x61, 0x60, 0x68,
0x70, 0x61, 0xA0, 0x68, 0x30, 0x61, 0xE0, 0x68, 0xF0, 0x60, 0x28, 0x09,
0xB0, 0x60, 0x70, 0x68, 0x40, 0xF4, 0x00, 0x40, 0x70, 0x60, 0x70, 0x68,
0x40, 0xF4, 0x00, 0x20, 0x70, 0x60, 0x05, 0x20, 0xFF, 0xF7, 0xBB, 0xFF,
0x70, 0x68, 0x40, 0xF4, 0x80, 0x10, 0x70, 0x60, 0x0A, 0x20, 0xFF, 0xF7,
0xB4, 0xFF, 0x70, 0x68, 0x40, 0xF4, 0x80, 0x30, 0x70, 0x60, 0x14, 0x20,
0xFF, 0xF7, 0xAD, 0xFF, 0x70, 0x68, 0x20, 0xF4, 0x80, 0x30, 0x70, 0x60,
0x70, 0x68, 0x20, 0xF4, 0x00, 0x20, 0x70, 0x60, 0x05, 0x20, 0xFF, 0xF7,
0xA2, 0xFF, 0x70, 0x68, 0x20, 0xF4, 0x80, 0x10, 0x70, 0x60, 0x70, 0x68,
0x20, 0xF4, 0x00, 0x40, 0x70, 0x60, 0x0A, 0x20, 0xFF, 0xF7, 0x97, 0xFF,
0x70, 0x68, 0x20, 0xF0, 0x70, 0x50, 0x70, 0x60, 0xE8, 0x06, 0x7C, 0xD4,
0xE1, 0x68, 0x05, 0xF1, 0x0C, 0x00, 0xFF, 0xF7, 0x13, 0xFF, 0xA1, 0x68,
0x4F, 0xEA, 0x00, 0x6C, 0x05, 0xF1, 0x08, 0x00, 0xFF, 0xF7, 0x0C, 0xFF,
0x61, 0x68, 0x4C, 0xEA, 0x00, 0x4C, 0x28, 0x1D, 0xFF, 0xF7, 0x06, 0xFF,
0x21, 0x68, 0x4C, 0xEA, 0x00, 0x2C, 0x28, 0x46, 0xFF, 0xF7, 0x00, 0xFF,
0xE1, 0x69, 0x4C, 0xEA, 0x00, 0x0A, 0x05, 0xF1, 0x1C, 0x00, 0xFF, 0xF7,
0xF9, 0xFE, 0xA1, 0x69, 0x4F, 0xEA, 0x00, 0x6C, 0x05, 0xF1, 0x18, 0x00,
0xFF, 0xF7, 0xF2, 0xFE, 0x61, 0x69, 0x4C, 0xEA, 0x00, 0x4C, 0x05, 0xF1,
0x14, 0x00, 0xFF, 0xF7, 0xEB, 0xFE, 0x21, 0x69, 0x4C, 0xEA, 0x00, 0x28,
0x05, 0xF1, 0x10, 0x00, 0xFF, 0xF7, 0xE4, 0xFE, 0x48, 0xEA, 0x00, 0x08,
0x70, 0x68, 0x40, 0xF0, 0x40, 0x40, 0x70, 0x60, 0x68, 0x09, 0xB0, 0x60,
0xC6, 0xF8, 0x1C, 0xA0, 0xC6, 0xF8, 0x20, 0x80, 0x70, 0x68, 0x40, 0xF4,
0x00, 0x40, 0x70, 0x60, 0x70, 0x68, 0x40, 0xF4, 0x00, 0x20, 0x70, 0x60,
0x05, 0x20, 0xFF, 0xF7, 0x46, 0xFF, 0x70, 0x68, 0x40, 0xF4, 0x80, 0x10,
0x70, 0x60, 0x0A, 0x20, 0xFF, 0xF7, 0x3F, 0xFF, 0x70, 0x68, 0x40, 0xF4,
0x80, 0x30, 0x70, 0x60, 0x14, 0x20, 0xFF, 0xF7, 0x38, 0xFF, 0x70, 0x68,
0x20, 0xF4, 0x80, 0x30, 0x70, 0x60, 0x70, 0x68, 0x20, 0xF4, 0x00, 0x20,
0x70, 0x60, 0x05, 0x20, 0xFF, 0xF7, 0x2D, 0xFF, 0x70, 0x68, 0x20, 0xF4,
0x80, 0x10, 0x70, 0x60, 0x70, 0x68, 0x20, 0xF4, 0x00, 0x40, 0x70, 0x60,
0x0A, 0x20, 0xFF, 0xF7, 0x22, 0xFF, 0x70, 0x68, 0x20, 0xF0, 0x40, 0x40,
0x70, 0x60, 0x70, 0x68, 0x20, 0xF4, 0x00, 0x10, 0x70, 0x60, 0x70, 0x68,
0x20, 0xF4, 0x80, 0x70, 0x70, 0x60, 0x70, 0x68, 0x00, 0xE0, 0x02, 0xE0,
0x20, 0xF0, 0x10, 0x00, 0x70, 0x60, 0x10, 0x34, 0x10, 0x35, 0x10, 0x3F,
0x70, 0x68, 0x00, 0x2F, 0x7F, 0xF4, 0x22, 0xAF, 0x20, 0xF0, 0x40, 0x40,
0x70, 0x60, 0x70, 0x68, 0x20, 0xF4, 0x00, 0x10, 0x70, 0x60, 0x70, 0x68,
0x20, 0xF4, 0x80, 0x70, 0x70, 0x60, 0x70, 0x68, 0x20, 0xF0, 0x10, 0x00,
0x70, 0x60, 0x00, 0x20, 0x30, 0x60, 0x00, 0xBE, 0xBD, 0xE8, 0xF0, 0x8D,
0x4E, 0x4B, 0x10, 0xB5, 0x5C, 0x68, 0x44, 0xF0, 0x10, 0x04, 0x5C, 0x60,
0x5C, 0x68, 0x44, 0xF4, 0x80, 0x74, 0x5C, 0x60, 0x5C, 0x68, 0x24, 0xF4,
0xFC, 0x44, 0x5C, 0x60, 0x5C, 0x68, 0x24, 0xF4, 0x78, 0x14, 0x5C, 0x60,
0x5C, 0x68, 0x24, 0xF4, 0xC0, 0x34, 0x5C, 0x60, 0x5C, 0x68, 0x24, 0xF0,
0x7C, 0x44, 0x5C, 0x60, 0x5C, 0x68, 0x44, 0xEA, 0x40, 0x50, 0x58, 0x60,
0x58, 0x68, 0x40, 0xF0, 0x70, 0x50, 0x58, 0x60, 0x08, 0x09, 0x98, 0x60,
0x58, 0x68, 0x40, 0xF4, 0xC0, 0x30, 0x58, 0x60, 0x58, 0x68, 0x40, 0xF4,
0xF0, 0x50, 0x58, 0x60, 0x58, 0x68, 0x40, 0xF4, 0x80, 0x00, 0x58, 0x60,
0x58, 0x68, 0x20, 0xF4, 0xF0, 0x50, 0x58, 0x60, 0x58, 0x68, 0x20, 0xF4,
0xC0, 0x30, 0x58, 0x60, 0x88, 0x08, 0x80, 0x07, 0x78, 0xD0, 0x18, 0x6B,
0x98, 0x61, 0xC1, 0xF3, 0x81, 0x00, 0x01, 0x28, 0x74, 0xD0, 0xDC, 0x6A,
0x5C, 0x61, 0x02, 0x28, 0x72, 0xD0, 0x98, 0x6A, 0x18, 0x61, 0x6F, 0xEA,
0x91, 0x00, 0x80, 0x07, 0x00, 0xD0, 0x5A, 0x6A, 0xDA, 0x60, 0x58, 0x68,
0x40, 0xF4, 0x00, 0x40, 0x58, 0x60, 0x58, 0x68, 0x40, 0xF4, 0x00, 0x20,
0x58, 0x60, 0x05, 0x20, 0xFF, 0xF7, 0x9D, 0xFE, 0x58, 0x68, 0x40, 0xF4,
0x80, 0x10, 0x58, 0x60, 0x0A, 0x20, 0xFF, 0xF7, 0x96, 0xFE, 0x58, 0x68,
0x40, 0xF4, 0x80, 0x30, 0x58, 0x60, 0x14, 0x20, 0xFF, 0xF7, 0x8F, 0xFE,
0x58, 0x68, 0x20, 0xF4, 0x80, 0x30, 0x58, 0x60, 0x58, 0x68, 0x20, 0xF4,
0x00, 0x20, 0x58, 0x60, 0x05, 0x20, 0xFF, 0xF7, 0x84, 0xFE, 0x58, 0x68,
0x20, 0xF4, 0x80, 0x10, 0x58, 0x60, 0x58, 0x68, 0x20, 0xF4, 0x00, 0x40,
0x58, 0x60, 0x0A, 0x20, 0xFF, 0xF7, 0x79, 0xFE, 0x58, 0x68, 0x21, 0xE0,
0x32, 0x64, 0xC8, 0x91, 0x34, 0x68, 0xD0, 0xA1, 0x38, 0x70, 0xE0, 0xC1,
0xD3, 0xA7, 0x4F, 0x9E, 0x54, 0xA8, 0x51, 0xA2, 0x58, 0xB0, 0x61, 0xC2,
0x98, 0x31, 0x62, 0xC4, 0x94, 0x29, 0x52, 0xA4, 0x43, 0x86, 0x0D, 0x1A,
0x83, 0x07, 0x0E, 0x1C, 0x3D, 0x7A, 0xF4, 0xE9, 0x45, 0x8A, 0x15, 0x2A,
0x85, 0x0B, 0x16, 0x2C, 0x89, 0x13, 0x26, 0x4C, 0x49, 0x92, 0x25, 0x4A,
0xA1, 0xAA, 0x55, 0x85, 0x00, 0x60, 0x00, 0x40, 0x20, 0xF0, 0x7C, 0x40,
0x58, 0x60, 0x58, 0x68, 0x20, 0xF4, 0x00, 0x10, 0x58, 0x60, 0x58, 0x68,
0x20, 0xF4, 0x80, 0x70, 0x58, 0x60, 0x58, 0x68, 0x20, 0xF0, 0x10, 0x00,
0x58, 0x60, 0x00, 0x20, 0x10, 0xBD, 0x01, 0xE0, 0x02, 0xE0, 0x03, 0xE0,
0x10, 0x46, 0x85, 0xE7, 0x14, 0x46, 0x89, 0xE7, 0x10, 0x46, 0x8B, 0xE7,
0x2D, 0xE9, 0xF0, 0x41, 0x8C, 0x4C, 0x06, 0x46, 0x8A, 0x48, 0xDD, 0xF8,
0x18, 0x80, 0x9C, 0x46, 0x15, 0x46, 0x0F, 0x46, 0x20, 0x60, 0x32, 0x46,
0x1C, 0x21, 0x01, 0x20, 0xFF, 0xF7, 0x36, 0xFF, 0x3A, 0x46, 0x2C, 0x21,
0x01, 0x20, 0xFF, 0xF7, 0x31, 0xFF, 0x2A, 0x46, 0x3C, 0x21, 0x01, 0x20,
0xFF, 0xF7, 0x2C, 0xFF, 0x62, 0x46, 0x31, 0x46, 0x00, 0x20, 0xFF, 0xF7,
0x27, 0xFF, 0x42, 0x46, 0x39, 0x46, 0x00, 0x20, 0xFF, 0xF7, 0x22, 0xFF,
0x4F, 0xF0, 0x80, 0x72, 0x60, 0x68, 0x13, 0x11, 0x4F, 0xF0, 0xFF, 0x31,
0x40, 0xF0, 0x10, 0x00, 0x60, 0x60, 0x60, 0x68, 0x40, 0xF4, 0x80, 0x70,
0x60, 0x60, 0x60, 0x68, 0x20, 0xF4, 0xFC, 0x40, 0x60, 0x60, 0x60, 0x68,
0x20, 0xF4, 0x78, 0x10, 0x60, 0x60, 0x60, 0x68, 0x40, 0xF4, 0xC0, 0x30,
0x60, 0x60, 0x60, 0x68, 0x40, 0xF0, 0x7C, 0x40, 0x60, 0x60, 0x10, 0x09,
0xA0, 0x60, 0x60, 0x68, 0x40, 0xF4, 0xF0, 0x50, 0x60, 0x60, 0x60, 0x68,
0x40, 0xF4, 0x80, 0x00, 0x60, 0x60, 0x60, 0x68, 0x20, 0xF4, 0xF0, 0x50,
0x60, 0x60, 0xAA, 0x42, 0x7E, 0xD0, 0x26, 0x6B, 0x08, 0x0A, 0x80, 0xEA,
0x06, 0x67, 0x64, 0x48, 0xC9, 0xB2, 0x78, 0x44, 0x4F, 0xEA, 0x16, 0x2C,
0x50, 0xF8, 0x21, 0x10, 0x4F, 0x40, 0x39, 0x0A, 0xFF, 0xB2, 0x81, 0xEA,
0x0C, 0x61, 0x50, 0xF8, 0x27, 0x70, 0x4F, 0xEA, 0x16, 0x4C, 0x79, 0x40,
0x0F, 0x0A, 0xC9, 0xB2, 0x87, 0xEA, 0x0C, 0x67, 0x50, 0xF8, 0x21, 0x10,
0x4F, 0x40, 0x06, 0xF0, 0x7F, 0x41, 0xFE, 0xB2, 0x81, 0xEA, 0x17, 0x21,
0x50, 0xF8, 0x26, 0x60, 0x71, 0x40, 0xE6, 0x6A, 0x0F, 0x0A, 0xC9, 0xB2,
0x87, 0xEA, 0x06, 0x67, 0x50, 0xF8, 0x21, 0x10, 0x4F, 0xEA, 0x16, 0x2C,
0x4F, 0x40, 0x39, 0x0A, 0xFF, 0xB2, 0x81, 0xEA, 0x0C, 0x61, 0x50, 0xF8,
0x27, 0x70, 0x4F, 0xEA, 0x16, 0x4C, 0x79, 0x40, 0x0F, 0x0A, 0xC9, 0xB2,
0x87, 0xEA, 0x0C, 0x67, 0x50, 0xF8, 0x21, 0x10, 0x4F, 0x40, 0x06, 0xF0,
0x7F, 0x41, 0xFE, 0xB2, 0x81, 0xEA, 0x17, 0x21, 0x50, 0xF8, 0x26, 0x60,
0x71, 0x40, 0xA6, 0x6A, 0x0F, 0x0A, 0xC9, 0xB2, 0x87, 0xEA, 0x06, 0x67,
0x50, 0xF8, 0x21, 0x10, 0x4F, 0xEA, 0x16, 0x2C, 0x4F, 0x40, 0x39, 0x0A,
0xFF, 0xB2, 0x81, 0xEA, 0x0C, 0x61, 0x50, 0xF8, 0x27, 0x70, 0x4F, 0xEA,
0x16, 0x4C, 0x79, 0x40, 0x0F, 0x0A, 0xC9, 0xB2, 0x87, 0xEA, 0x0C, 0x67,
0x50, 0xF8, 0x21, 0x10, 0x4F, 0x40, 0x06, 0xF0, 0x7F, 0x41, 0xFE, 0xB2,
0x81, 0xEA, 0x17, 0x21, 0x50, 0xF8, 0x26, 0x60, 0x71, 0x40, 0x66, 0x6A,
0x0F, 0x0A, 0xC9, 0xB2, 0x87, 0xEA, 0x06, 0x67, 0x50, 0xF8, 0x21, 0x10,
0x4F, 0xEA, 0x16, 0x2C, 0x4F, 0x40, 0x39, 0x0A, 0xFF, 0xB2, 0x81, 0xEA,
0x0C, 0x61, 0x50, 0xF8, 0x27, 0x70, 0x4F, 0xEA, 0x16, 0x4C, 0x79, 0x40,
0x0F, 0x0A, 0x87, 0xEA, 0x0C, 0x67, 0x00, 0xE0, 0x0B, 0xE0, 0xC9, 0xB2,
0x50, 0xF8, 0x21, 0x10, 0x4F, 0x40, 0x06, 0xF0, 0x7F, 0x41, 0xFE, 0xB2,
0x81, 0xEA, 0x17, 0x21, 0x50, 0xF8, 0x26, 0x00, 0x41, 0x40, 0x10, 0x32,
0x10, 0x3B, 0x7F, 0xF4, 0x60, 0xAF, 0x60, 0x68, 0x20, 0xF0, 0x7C, 0x40,
0x60, 0x60, 0x60, 0x68, 0x20, 0xF4, 0xC0, 0x30, 0x60, 0x60, 0x60, 0x68,
0x20, 0xF4, 0x80, 0x70, 0x60, 0x60, 0x60, 0x68, 0x20, 0xF0, 0x10, 0x00,
0x60, 0x60, 0x4F, 0xF0, 0x55, 0x30, 0x00, 0xEA, 0x51, 0x00, 0x01, 0xF0,
0x55, 0x31, 0x40, 0xEA, 0x41, 0x00, 0x4F, 0xF0, 0x33, 0x31, 0x01, 0xEA,
0x90, 0x01, 0x00, 0xF0, 0x33, 0x30, 0x41, 0xEA, 0x80, 0x00, 0x4F, 0xF0,
0x0F, 0x31, 0x01, 0xEA, 0x10, 0x11, 0x00, 0xF0, 0x0F, 0x30, 0x41, 0xEA,
0x00, 0x10, 0x40, 0xBA, 0x4F, 0xEA, 0x30, 0x46, 0x32, 0x46, 0x29, 0x46,
0x00, 0x20, 0xFF, 0xF7, 0x35, 0xFE, 0xC4, 0xF8, 0x18, 0x80, 0x00, 0x20,
0x20, 0x60, 0x30, 0x46, 0x00, 0xBE, 0xBD, 0xE8, 0xF0, 0x81, 0x00, 0x00,
0xA1, 0xAA, 0x55, 0x85, 0x00, 0x60, 0x00, 0x40, 0x92, 0x01, 0x00, 0x00,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x96, 0x30, 0x07, 0x77,
0x2C, 0x61, 0x0E, 0xEE, 0xBA, 0x51, 0x09, 0x99, 0x19, 0xC4, 0x6D, 0x07,
0x8F, 0xF4, 0x6A, 0x70, 0x35, 0xA5, 0x63, 0xE9, 0xA3, 0x95, 0x64, 0x9E,
0x32, 0x88, 0xDB, 0x0E, 0xA4, 0xB8, 0xDC, 0x79, 0x1E, 0xE9, 0xD5, 0xE0,
0x88, 0xD9, 0xD2, 0x97, 0x2B, 0x4C, 0xB6, 0x09, 0xBD, 0x7C, 0xB1, 0x7E,
0x07, 0x2D, 0xB8, 0xE7, 0x91, 0x1D, 0xBF, 0x90, 0x64, 0x10, 0xB7, 0x1D,
0xF2, 0x20, 0xB0, 0x6A, 0x48, 0x71, 0xB9, 0xF3, 0xDE, 0x41, 0xBE, 0x84,
0x7D, 0xD4, 0xDA, 0x1A, 0xEB, 0xE4, 0xDD, 0x6D, 0x51, 0xB5, 0xD4, 0xF4,
0xC7, 0x85, 0xD3, 0x83, 0x56, 0x98, 0x6C, 0x13, 0xC0, 0xA8, 0x6B, 0x64,
0x7A, 0xF9, 0x62, 0xFD, 0xEC, 0xC9, 0x65, 0x8A, 0x4F, 0x5C, 0x01, 0x14,
0xD9, 0x6C, 0x06, 0x63, 0x63, 0x3D, 0x0F, 0xFA, 0xF5, 0x0D, 0x08, 0x8D,
0xC8, 0x20, 0x6E, 0x3B, 0x5E, 0x10, 0x69, 0x4C, 0xE4, 0x41, 0x60, 0xD5,
0x72, 0x71, 0x67, 0xA2, 0xD1, 0xE4, 0x03, 0x3C, 0x47, 0xD4, 0x04, 0x4B,
0xFD, 0x85, 0x0D, 0xD2, 0x6B, 0xB5, 0x0A, 0xA5, 0xFA, 0xA8, 0xB5, 0x35,
0x6C, 0x98, 0xB2, 0x42, 0xD6, 0xC9, 0xBB, 0xDB, 0x40, 0xF9, 0xBC, 0xAC,
0xE3, 0x6C, 0xD8, 0x32, 0x75, 0x5C, 0xDF, 0x45, 0xCF, 0x0D, 0xD6, 0xDC,
0x59, 0x3D, 0xD1, 0xAB, 0xAC, 0x30, 0xD9, 0x26, 0x3A, 0x00, 0xDE, 0x51,
0x80, 0x51, 0xD7, 0xC8, 0x16, 0x61, 0xD0, 0xBF, 0xB5, 0xF4, 0xB4, 0x21,
0x23, 0xC4, 0xB3, 0x56, 0x99, 0x95, 0xBA, 0xCF, 0x0F, 0xA5, 0xBD, 0xB8,
0x9E, 0xB8, 0x02, 0x28, 0x08, 0x88, 0x05, 0x5F, 0xB2, 0xD9, 0x0C, 0xC6,
0x24, 0xE9, 0x0B, 0xB1, 0x87, 0x7C, 0x6F, 0x2F, 0x11, 0x4C, 0x68, 0x58,
0xAB, 0x1D, 0x61, 0xC1, 0x3D, 0x2D, 0x66, 0xB6, 0x90, 0x41, 0xDC, 0x76,
0x06, 0x71, 0xDB, 0x01, 0xBC, 0x20, 0xD2, 0x98, 0x2A, 0x10, 0xD5, 0xEF,
0x89, 0x85, 0xB1, 0x71, 0x1F, 0xB5, 0xB6, 0x06, 0xA5, 0xE4, 0xBF, 0x9F,
0x33, 0xD4, 0xB8, 0xE8, 0xA2, 0xC9, 0x07, 0x78, 0x34, 0xF9, 0x00, 0x0F,
0x8E, 0xA8, 0x09, 0x96, 0x18, 0x98, 0x0E, 0xE1, 0xBB, 0x0D, 0x6A, 0x7F,
0x2D, 0x3D, 0x6D, 0x08, 0x97, 0x6C, 0x64, 0x91, 0x01, 0x5C, 0x63, 0xE6,
0xF4, 0x51, 0x6B, 0x6B, 0x62, 0x61, 0x6C, 0x1C, 0xD8, 0x30, 0x65, 0x85,
0x4E, 0x00, 0x62, 0xF2, 0xED, 0x95, 0x06, 0x6C, 0x7B, 0xA5, 0x01, 0x1B,
0xC1, 0xF4, 0x08, 0x82, 0x57, 0xC4, 0x0F, 0xF5, 0xC6, 0xD9, 0xB0, 0x65,
0x50, 0xE9, 0xB7, 0x12, 0xEA, 0xB8, 0xBE, 0x8B, 0x7C, 0x88, 0xB9, 0xFC,
0xDF, 0x1D, 0xDD, 0x62, 0x49, 0x2D, 0xDA, 0x15, 0xF3, 0x7C, 0xD3, 0x8C,
0x65, 0x4C, 0xD4, 0xFB, 0x58, 0x61, 0xB2, 0x4D, 0xCE, 0x51, 0xB5, 0x3A,
0x74, 0x00, 0xBC, 0xA3, 0xE2, 0x30, 0xBB, 0xD4, 0x41, 0xA5, 0xDF, 0x4A,
0xD7, 0x95, 0xD8, 0x3D, 0x6D, 0xC4, 0xD1, 0xA4, 0xFB, 0xF4, 0xD6, 0xD3,
0x6A, 0xE9, 0x69, 0x43, 0xFC, 0xD9, 0x6E, 0x34, 0x46, 0x88, 0x67, 0xAD,
0xD0, 0xB8, 0x60, 0xDA, 0x73, 0x2D, 0x04, 0x44, 0xE5, 0x1D, 0x03, 0x33,
0x5F, 0x4C, 0x0A, 0xAA, 0xC9, 0x7C, 0x0D, 0xDD, 0x3C, 0x71, 0x05, 0x50,
0xAA, 0x41, 0x02, 0x27, 0x10, 0x10, 0x0B, 0xBE, 0x86, 0x20, 0x0C, 0xC9,
0x25, 0xB5, 0x68, 0x57, 0xB3, 0x85, 0x6F, 0x20, 0x09, 0xD4, 0x66, 0xB9,
0x9F, 0xE4, 0x61, 0xCE, 0x0E, 0xF9, 0xDE, 0x5E, 0x98, 0xC9, 0xD9, 0x29,
0x22, 0x98, 0xD0, 0xB0, 0xB4, 0xA8, 0xD7, 0xC7, 0x17, 0x3D, 0xB3, 0x59,
0x81, 0x0D, 0xB4, 0x2E, 0x3B, 0x5C, 0xBD, 0xB7, 0xAD, 0x6C, 0xBA, 0xC0,
0x20, 0x83, 0xB8, 0xED, 0xB6, 0xB3, 0xBF, 0x9A, 0x0C, 0xE2, 0xB6, 0x03,
0x9A, 0xD2, 0xB1, 0x74, 0x39, 0x47, 0xD5, 0xEA, 0xAF, 0x77, 0xD2, 0x9D,
0x15, 0x26, 0xDB, 0x04, 0x83, 0x16, 0xDC, 0x73, 0x12, 0x0B, 0x63, 0xE3,
0x84, 0x3B, 0x64, 0x94, 0x3E, 0x6A, 0x6D, 0x0D, 0xA8, 0x5A, 0x6A, 0x7A,
0x0B, 0xCF, 0x0E, 0xE4, 0x9D, 0xFF, 0x09, 0x93, 0x27, 0xAE, 0x00, 0x0A,
0xB1, 0x9E, 0x07, 0x7D, 0x44, 0x93, 0x0F, 0xF0, 0xD2, 0xA3, 0x08, 0x87,
0x68, 0xF2, 0x01, 0x1E, 0xFE, 0xC2, 0x06, 0x69, 0x5D, 0x57, 0x62, 0xF7,
0xCB, 0x67, 0x65, 0x80, 0x71, 0x36, 0x6C, 0x19, 0xE7, 0x06, 0x6B, 0x6E,
0x76, 0x1B, 0xD4, 0xFE, 0xE0, 0x2B, 0xD3, 0x89, 0x5A, 0x7A, 0xDA, 0x10,
0xCC, 0x4A, 0xDD, 0x67, 0x6F, 0xDF, 0xB9, 0xF9, 0xF9, 0xEF, 0xBE, 0x8E,
0x43, 0xBE, 0xB7, 0x17, 0xD5, 0x8E, 0xB0, 0x60, 0xE8, 0xA3, 0xD6, 0xD6,
0x7E, 0x93, 0xD1, 0xA1, 0xC4, 0xC2, 0xD8, 0x38, 0x52, 0xF2, 0xDF, 0x4F,
0xF1, 0x67, 0xBB, 0xD1, 0x67, 0x57, 0xBC, 0xA6, 0xDD, 0x06, 0xB5, 0x3F,
0x4B, 0x36, 0xB2, 0x48, 0xDA, 0x2B, 0x0D, 0xD8, 0x4C, 0x1B, 0x0A, 0xAF,
0xF6, 0x4A, 0x03, 0x36, 0x60, 0x7A, 0x04, 0x41, 0xC3, 0xEF, 0x60, 0xDF,
0x55, 0xDF, 0x67, 0xA8, 0xEF, 0x8E, 0x6E, 0x31, 0x79, 0xBE, 0x69, 0x46,
0x8C, 0xB3, 0x61, 0xCB, 0x1A, 0x83, 0x66, 0xBC, 0xA0, 0xD2, 0x6F, 0x25,
0x36, 0xE2, 0x68, 0x52, 0x95, 0x77, 0x0C, 0xCC, 0x03, 0x47, 0x0B, 0xBB,
0xB9, 0x16, 0x02, 0x22, 0x2F, 0x26, 0x05, 0x55, 0xBE, 0x3B, 0xBA, 0xC5,
0x28, 0x0B, 0xBD, 0xB2, 0x92, 0x5A, 0xB4, 0x2B, 0x04, 0x6A, 0xB3, 0x5C,
0xA7, 0xFF, 0xD7, 0xC2, 0x31, 0xCF, 0xD0, 0xB5, 0x8B, 0x9E, 0xD9, 0x2C,
0x1D, 0xAE, 0xDE, 0x5B, 0xB0, 0xC2, 0x64, 0x9B, 0x26, 0xF2, 0x63, 0xEC,
0x9C, 0xA3, 0x6A, 0x75, 0x0A, 0x93, 0x6D, 0x02, 0xA9, 0x06, 0x09, 0x9C,
0x3F, 0x36, 0x0E, 0xEB, 0x85, 0x67, 0x07, 0x72, 0x13, 0x57, 0x00, 0x05,
0x82, 0x4A, 0xBF, 0x95, 0x14, 0x7A, 0xB8, 0xE2, 0xAE, 0x2B, 0xB1, 0x7B,
0x38, 0x1B, 0xB6, 0x0C, 0x9B, 0x8E, 0xD2, 0x92, 0x0D, 0xBE, 0xD5, 0xE5,
0xB7, 0xEF, 0xDC, 0x7C, 0x21, 0xDF, 0xDB, 0x0B, 0xD4, 0xD2, 0xD3, 0x86,
0x42, 0xE2, 0xD4, 0xF1, 0xF8, 0xB3, 0xDD, 0x68, 0x6E, 0x83, 0xDA, 0x1F,
0xCD, 0x16, 0xBE, 0x81, 0x5B, 0x26, 0xB9, 0xF6, 0xE1, 0x77, 0xB0, 0x6F,
0x77, 0x47, 0xB7, 0x18, 0xE6, 0x5A, 0x08, 0x88, 0x70, 0x6A, 0x0F, 0xFF,
0xCA, 0x3B, 0x06, 0x66, 0x5C, 0x0B, 0x01, 0x11, 0xFF, 0x9E, 0x65, 0x8F,
0x69, 0xAE, 0x62, 0xF8, 0xD3, 0xFF, 0x6B, 0x61, 0x45, 0xCF, 0x6C, 0x16,
0x78, 0xE2, 0x0A, 0xA0, 0xEE, 0xD2, 0x0D, 0xD7, 0x54, 0x83, 0x04, 0x4E,
0xC2, 0xB3, 0x03, 0x39, 0x61, 0x26, 0x67, 0xA7, 0xF7, 0x16, 0x60, 0xD0,
0x4D, 0x47, 0x69, 0x49, 0xDB, 0x77, 0x6E, 0x3E, 0x4A, 0x6A, 0xD1, 0xAE,
0xDC, 0x5A, 0xD6, 0xD9, 0x66, 0x0B, 0xDF, 0x40, 0xF0, 0x3B, 0xD8, 0x37,
0x53, 0xAE, 0xBC, 0xA9, 0xC5, 0x9E, 0xBB, 0xDE, 0x7F, 0xCF, 0xB2, 0x47,
0xE9, 0xFF, 0xB5, 0x30, 0x1C, 0xF2, 0xBD, 0xBD, 0x8A, 0xC2, 0xBA, 0xCA,
0x30, 0x93, 0xB3, 0x53, 0xA6, 0xA3, 0xB4, 0x24, 0x05, 0x36, 0xD0, 0xBA,
0x93, 0x06, 0xD7, 0xCD, 0x29, 0x57, 0xDE, 0x54, 0xBF, 0x67, 0xD9, 0x23,
0x2E, 0x7A, 0x66, 0xB3, 0xB8, 0x4A, 0x61, 0xC4, 0x02, 0x1B, 0x68, 0x5D,
0x94, 0x2B, 0x6F, 0x2A, 0x37, 0xBE, 0x0B, 0xB4, 0xA1, 0x8E, 0x0C, 0xC3,
0x1B, 0xDF, 0x05, 0x5A, 0x8D, 0xEF, 0x02, 0x2D, 0x00, 0x00, 0x00, 0x00,
};

uint32_t crc32_32r(uint32_t crc, uint32_t val)
{
    crc = (crc >> 8) ^ rCrc32Tab[(crc & 0xff)] ^ ((val >> 0) << 24);
    crc = (crc >> 8) ^ rCrc32Tab[(crc & 0xff)] ^ ((val >> 8) << 24);
    crc = (crc >> 8) ^ rCrc32Tab[(crc & 0xff)] ^ ((val >> 16) << 24);
    crc = (crc >> 8) ^ rCrc32Tab[(crc & 0xff)] ^ ((val >> 24) << 24);
    return crc;
}

uint32_t reverse(uint32_t x)
{
    x = (((x & 0xaaaaaaaa) >> 1) | ((x & 0x55555555) << 1));
    x = (((x & 0xcccccccc) >> 2) | ((x & 0x33333333) << 2));
    x = (((x & 0xf0f0f0f0) >> 4) | ((x & 0x0f0f0f0f) << 4));
    x = (((x & 0xff00ff00) >> 8) | ((x & 0x00ff00ff) << 8));
    return ((x >> 16) | (x << 16));
}

int ProgramWord(struct flash_bank *bank, bool mem_type, uint32_t adr, uint32_t data)
{
    struct target *target = bank->target;

    uint32_t flash_cmd; //, cur_per_clock;
    int retval, retval2;

    if (target->state != TARGET_HALTED) {
        LOG_ERROR("Target not halted");
        return ERROR_TARGET_NOT_HALTED;
    }

    retval = target_write_u32(target, FLASH_KEY, KEY);
    if (retval != ERROR_OK)
        goto lock;

    retval = target_read_u32(target, FLASH_CMD, &flash_cmd);
    if (retval != ERROR_OK)
        goto lock;

    /* Switch on register access */
    flash_cmd = (flash_cmd & (FLASH_CNTR_WAIT_Msk | FLASH_CNTR_ERASE_START_Msk)) |
                FLASH_CNTR_MODE_Msk | FLASH_CNTR_TMR_Msk;
    if (mem_type)
        flash_cmd |= FLASH_CNTR_IFREN_Msk;
    retval = target_write_u32(target, FLASH_CMD, flash_cmd);
    if (retval != ERROR_OK)
        goto reset_pg_and_lock;
    flash_cmd |= FLASH_CNTR_MODE_Msk;
    flash_cmd |= FLASH_CNTR_TMR_Msk;
    flash_cmd &= ~FLASH_CNTR_SE50_Msk; //SE[5:0]=0
    flash_cmd &= ~(FLASH_CNTR_NVSTR_Msk | FLASH_CNTR_PROG_Msk | FLASH_CNTR_MAS1_Msk |
                   FLASH_CNTR_ERASE_Msk); //NVSTR=0, PROG=0, MAS1=0,ERASE=0
    flash_cmd |= FLASH_CNTR_FLASH30_CR_Msk; //SEL[3:0]=1
    UPD(flash_cmd);

    retval = target_write_u32(target, FLASH_ADR, adr >> 4);
    if (retval != ERROR_OK)
        goto reset_pg_and_lock;

    // read data

    flash_cmd |= FLASH_CNTR_YE_Msk | FLASH_CNTR_XE_Msk; //YE=1,XE=1
    UPD(flash_cmd); // -10 ns
    flash_cmd |= FLASH_CNTR_SE30_Msk; //SE[3:0]=1 // 0xF
    UPD(flash_cmd);
    flash_cmd |= FLASH_CNTR_rdata_ready_Msk; //rdata_ready=1
    UPD(flash_cmd);
    //    flash_cmd &= ~FLASH_CNTR_rdata_ready_Msk;//rdata_ready=0
    flash_cmd &= ~FLASH_CNTR_SE30_Msk; //SE[3:0]=0 // 0xF
    flash_cmd &= ~(FLASH_CNTR_YE_Msk | FLASH_CNTR_XE_Msk); //YE=0,XE=0
    UPD(flash_cmd);

    uint32_t DATA[4];

    retval = target_read_u32(target, FLASH_RDATA0, &DATA[0]);
    if (retval != ERROR_OK)
        goto reset_pg_and_lock;
    retval = target_read_u32(target, FLASH_RDATA1, &DATA[1]);
    if (retval != ERROR_OK)
        goto reset_pg_and_lock;
    retval = target_read_u32(target, FLASH_RDATA2, &DATA[2]);
    if (retval != ERROR_OK)
        goto reset_pg_and_lock;
    retval = target_read_u32(target, FLASH_RDATA3, &DATA[3]);
    if (retval != ERROR_OK)
        goto reset_pg_and_lock;

    DATA[(adr >> 2) & 3] = data;

    //write data

    retval = target_write_u32(target, FLASH_WDATA0, DATA[0]);
    if (retval != ERROR_OK)
        goto reset_pg_and_lock;
    retval = target_write_u32(target, FLASH_WDATA1, DATA[1]);
    if (retval != ERROR_OK)
        goto reset_pg_and_lock;
    retval = target_write_u32(target, FLASH_WDATA2, DATA[2]);
    if (retval != ERROR_OK)
        goto reset_pg_and_lock;
    retval = target_write_u32(target, FLASH_WDATA3, DATA[3]);
    if (retval != ERROR_OK)
        goto reset_pg_and_lock;

    flash_cmd |= FLASH_CNTR_XE_Msk; //XE=1
    UPD(flash_cmd); // -10 ns
    flash_cmd |= FLASH_CNTR_PROG_Msk; //PROG=1
    UPD(flash_cmd);
    usleep(5); /* Wait for 5 us */
    flash_cmd |= FLASH_CNTR_NVSTR_Msk; //NVSTR = 1
    UPD(flash_cmd);
    usleep(10); /* Wait for 10 us */
    flash_cmd |= FLASH_CNTR_YE_Msk; //YE=1
    UPD(flash_cmd);
    usleep(20); /* Wait for 20 us */
    flash_cmd &= ~(FLASH_CNTR_YE_Msk); //YE=0
    UPD(flash_cmd); // 20 ns
    flash_cmd &= ~(FLASH_CNTR_PROG_Msk); //PROG=0
    UPD(flash_cmd);
    usleep(5); /* Wait for 5 us */
    flash_cmd &= ~(FLASH_CNTR_NVSTR_Msk); //NVSTR = 0
    UPD(flash_cmd); // -10 ns
    flash_cmd &= ~(FLASH_CNTR_XE_Msk); //XE=0
    UPD(flash_cmd);
    usleep(10); /* Wait for 10 us */
    flash_cmd &= ~(FLASH_CNTR_FLASH30_CR_Msk); //SEL[3:0]=0
    UPD(flash_cmd);
    //
    //  flash_cmd &= ~(FLASH_CNTR_FLASH54_CR_Msk);//SEL[5:4]=0
    //
    //  flash_cmd &= ~(FLASH_CNTR_IFREN_Msk);//IFREN=0
    //  flash_cmd &= ~FLASH_CNTR_TMR_Msk;
    //  flash_cmd &= ~FLASH_CNTR_MODE_Msk;

    //  return (0);                                   // Done

reset_pg_and_lock:
    flash_cmd &= (FLASH_CNTR_WAIT_Msk | FLASH_CNTR_ERASE_START_Msk);
    retval2 = target_write_u32(target, FLASH_CMD, flash_cmd);
    if (retval == ERROR_OK)
        retval = retval2;
lock:
    retval2 = target_write_u32(target, FLASH_KEY, 0x0);
    if (retval == ERROR_OK)
        retval = retval2;

    return retval;
}

unsigned long FlashLock(struct flash_bank *bank, uint32_t key)
{
    struct target *target = bank->target;
    struct working_area *write_algorithm;
    struct working_area *write_algorithm_sp;
    const uint32_t stack_size = 128;
    uint32_t crc;
    uint32_t flash_cmd; //, cur_per_clock;
    int retval, retval2;

    uint32_t key1_addr = 0x010fffd0;
    uint32_t key2_addr = 0x010fffe0;
    uint32_t crc_addr = 0x010ffff0;
    uint32_t key1 = key & 0xFFFF0000; //0x12340000;
    uint32_t key2 = key & 0x0000FFFF; //0x00005678;
    //TODO: randomize key

    if (target->state != TARGET_HALTED) {
        LOG_ERROR("Target not halted");
        return ERROR_TARGET_NOT_HALTED;
    }

///////////////////////////////////
    /* flash write code */
    LOG_DEBUG("MDR149: request  0x%" PRIx32 " bytes", (unsigned int)sizeof(mdr149_code));
    if (target_alloc_working_area(target, sizeof(mdr149_code), &write_algorithm) != ERROR_OK ||
        target_alloc_working_area(target, stack_size, &write_algorithm_sp) != ERROR_OK) {
        LOG_WARNING("no working area available, falling back to single memory accesses");

        ProgramWord(bank, 1, 0x1C, key1_addr);
        ProgramWord(bank, 1, 0x2C, key2_addr);
        ProgramWord(bank, 1, 0x3C, crc_addr);

        ProgramWord(bank, 0, key1_addr, key1);
        ProgramWord(bank, 0, key2_addr, key2);

        retval = target_write_u32(target, FLASH_KEY, KEY);
        if (retval != ERROR_OK)
            goto lock;

        retval = target_read_u32(target, FLASH_CMD, &flash_cmd);
        if (retval != ERROR_OK)
            goto lock;

        /* Switch on register access */
        flash_cmd = (flash_cmd & (FLASH_CNTR_WAIT_Msk | FLASH_CNTR_ERASE_START_Msk)) |
                    FLASH_CNTR_MODE_Msk | FLASH_CNTR_TMR_Msk;
        //if (mem_type)
        //    flash_cmd |= FLASH_CNTR_IFREN_Msk;
        retval = target_write_u32(target, FLASH_CMD, flash_cmd);
        if (retval != ERROR_OK)
            goto reset_pg_and_lock;

        flash_cmd |= FLASH_CNTR_MODE_Msk;
        flash_cmd |= FLASH_CNTR_TMR_Msk;
        flash_cmd &= ~FLASH_CNTR_SE50_Msk; //SE[5:0]=0
        flash_cmd &= ~(FLASH_CNTR_NVSTR_Msk | FLASH_CNTR_PROG_Msk | FLASH_CNTR_MAS1_Msk |
                       FLASH_CNTR_ERASE_Msk); //NVSTR=0, PROG=0, MAS1=0,ERASE=0
        flash_cmd |= FLASH_CNTR_XE_Msk | FLASH_CNTR_YE_Msk; //YE=1,XE=1
        UPD(flash_cmd); // -10 ns
        flash_cmd |= FLASH_CNTR_FLASH30_CR_Msk; //SEL[3:0]=1
        flash_cmd |= FLASH_CNTR_rdata_ready_Msk; //rdata_ready=1
        UPD(flash_cmd);

        // calc CRC
        uint32_t adr = 0x01000000;
        uint32_t sz = 0x00100000;
        crc = 0xFFFFFFFF;

        while (sz) {
            keep_alive();
            if ((sz & 0xFFF) == 0)
                LOG_INFO("MDR149: sz=0x%" PRIx32 "", sz);

            retval = target_write_u32(target, FLASH_ADR, adr >> 4);
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;

            // read data

            flash_cmd |= FLASH_CNTR_SE30_Msk; //SE[3:0]=1 // 0xF
            UPD(flash_cmd);
            //UPD(flash_cmd);
            //    flash_cmd &= ~FLASH_CNTR_rdata_ready_Msk;//rdata_ready=0
            flash_cmd &= ~FLASH_CNTR_SE30_Msk; //SE[3:0]=0 // 0xF
            //flash_cmd &=~(FLASH_CNTR_YE_Msk | FLASH_CNTR_XE_Msk);//YE=0,XE=0
            UPD(flash_cmd);

            uint32_t DATA[4];
            retval = target_read_u32(target, FLASH_RDATA0, &DATA[0]);
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;
            retval = target_read_u32(target, FLASH_RDATA1, &DATA[1]);
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;
            retval = target_read_u32(target, FLASH_RDATA2, &DATA[2]);
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;
            retval = target_read_u32(target, FLASH_RDATA3, &DATA[3]);
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;

            //        if (sz ==0x00100000) {
            //            LOG_INFO ("MDR149: %"PRIx32" %"PRIx32" %"PRIx32" %"PRIx32"", DATA[0], DATA[1], DATA[2], DATA[3]);
            ////! int target_read_memory(struct target *target, target_addr_t address, uint32_t size, uint32_t count, uint8_t *buffer)
            //            retval = target_read_memory(target, FLASH_RDATA3, 4, 4, (uint8_t *)DATA);
            //            LOG_INFO ("MDR149: %"PRIx32" %"PRIx32" %"PRIx32" %"PRIx32"", DATA[0], DATA[1], DATA[2], DATA[3]);
            //        }

            if (adr != crc_addr) {
                #if 0 //0=MLDR149
                        if (adr>=0x010e0000)
                        {
                            crc = crc32_32r(crc, 0);
                            crc = crc32_32r(crc, 0);
                        }
                        else
                #endif
                {
                    crc = crc32_32r(crc, DATA[0]);
                    crc = crc32_32r(crc, DATA[1]);
                }
                crc = crc32_32r(crc, DATA[2]);
                crc = crc32_32r(crc, DATA[3]);
                //MSG_1(("%08X %08X", crc, reverse(crc)));
            }
            // Go to next
            adr += 16;
            sz -= 16;
        }

        flash_cmd &= ~(FLASH_CNTR_FLASH50_CR_Msk); //SEL[5:4]=0
        flash_cmd &=
                ~(FLASH_CNTR_IFREN_Msk | FLASH_CNTR_XE_Msk | FLASH_CNTR_YE_Msk); //IFREN=0,YE=0,XE=0
        flash_cmd &= ~FLASH_CNTR_TMR_Msk;
        flash_cmd &= ~FLASH_CNTR_MODE_Msk;
        UPD(flash_cmd);

        crc = reverse(crc);
        ProgramWord(bank, 0, crc_addr, crc);

    } else {
        const uint32_t num_reg_params = 5; // max 4
        const uint32_t num_mem_params = 1;
        struct reg_param reg_params[num_reg_params];
        struct mem_param mem_params[num_mem_params];
        struct armv7m_algorithm armv7m_info;

        retval =
                target_write_buffer(target, write_algorithm->address, sizeof(mdr149_code), mdr149_code);
        if (retval != ERROR_OK)
            goto reset_pg_and_lock;

        init_reg_param(&reg_params[0], "r0", 32, PARAM_IN_OUT);
        init_reg_param(&reg_params[1], "r1", 32, PARAM_OUT);
        init_reg_param(&reg_params[2], "r2", 32, PARAM_OUT);
        init_reg_param(&reg_params[3], "r3", 32, PARAM_OUT);
        init_reg_param(&reg_params[4], "sp", 32, PARAM_OUT);
        init_mem_param(&mem_params[0], write_algorithm_sp->address+stack_size-4, 32, PARAM_OUT);


        buf_set_u32(reg_params[0].value, 0, 32, key1_addr);
        buf_set_u32(reg_params[1].value, 0, 32, key2_addr);
        buf_set_u32(reg_params[2].value, 0, 32, crc_addr);
        buf_set_u32(reg_params[3].value, 0, 32, key1);
        buf_set_u32(reg_params[4].value, 0, 32, write_algorithm_sp->address+stack_size-4);
        buf_set_u32(mem_params[0].value, 0, 32, key2);

        armv7m_info.common_magic = ARMV7M_COMMON_MAGIC;
        armv7m_info.core_mode = ARM_MODE_THREAD;
        retval = target_run_algorithm(target, num_mem_params, mem_params, num_reg_params, reg_params,
                                      write_algorithm->address + 0x48d, 0, 5000, &armv7m_info);

        LOG_DEBUG("MDR149: status      0x%" PRIx32 "", buf_get_u32(reg_params[0].value, 0, 32));
        //        LOG_DEBUG("MDR149: word_count  0x%"PRIx32"", buf_get_u32(reg_params[1].value, 0, 32));
        //        LOG_DEBUG("MDR149: start       0x%"PRIx32"", buf_get_u32(reg_params[2].value, 0, 32));
        //        LOG_DEBUG("MDR149: end         0x%"PRIx32"", buf_get_u32(reg_params[3].value, 0, 32));
        //        LOG_DEBUG("MDR149: address     0x%"PRIx32"", buf_get_u32(reg_params[4].value, 0, 32));

        target_free_working_area(target, write_algorithm);
        target_free_working_area(target, write_algorithm_sp);

        for (unsigned int i = 0; i < num_reg_params; i++)
            destroy_reg_param(&reg_params[i]);
        if (retval == ERROR_FLASH_OPERATION_FAILED) {
            //            LOG_ERROR("flash write failed at address 0x%"PRIx32, buf_get_u32(reg_params[4].value, 0, 32));
            LOG_ERROR("MDR149: flash operation failed");
            goto reset_pg_and_lock;
        }
    }
///////////////////////////////////

reset_pg_and_lock:
    flash_cmd &= (FLASH_CNTR_WAIT_Msk | FLASH_CNTR_ERASE_START_Msk);
    retval2 = target_write_u32(target, FLASH_CMD, flash_cmd);
    if (retval == ERROR_OK)
        retval = retval2;
lock:
    retval2 = target_write_u32(target, FLASH_KEY, 0x0);
    if (retval == ERROR_OK)
        retval = retval2;

    return retval;
}

static int mdr149_mass_erase(struct flash_bank *bank)
{
    struct target *target = bank->target;
    struct mdr149_flash_bank *mdr149_info = bank->driver_priv;
    uint32_t flash_cmd;
    //uint32_t dumb;
    int retval;
    //unsigned int i;

    LOG_INFO("MDR149: ERASE ALL pages");

    retval = target_read_u32(target, FLASH_CMD, &flash_cmd);
    if (retval != ERROR_OK)
        return retval;

    flash_cmd = (flash_cmd & FLASH_CNTR_WAIT_Msk);
    if (mdr149_info->mem_type)
        flash_cmd |= FLASH_CNTR_IFREN_Msk;

    retval = target_write_u32(target, FLASH_CMD, flash_cmd); // for the case of already erased
    if (retval != ERROR_OK)
        return retval;

    flash_cmd |= FLASH_CNTR_ERASE_START_Msk;

    retval = target_write_u32(target, FLASH_CMD, flash_cmd);
    if (retval != ERROR_OK)
        return retval;

    uint32_t i = 0;
    uint32_t status;
    do {
        alive_sleep(100); // ms

        i++;
        if (i == 10)
            //{
            return 1;
        //}
        retval = target_read_u32(target, FLASH_CMD, &status);
        if (retval != ERROR_OK)
            continue;

    } while ((status & FLASH_CNTR_ERASE_DONE_Msk) != FLASH_CNTR_ERASE_DONE_Msk);

    return retval;
}

static int mdr149_erase(struct flash_bank *bank, unsigned int first, unsigned int last)
{
    struct target *target = bank->target;
    struct mdr149_flash_bank *mdr149_info = bank->driver_priv;
    int retval, retval2;
    //unsigned int j;
    uint32_t flash_cmd; //, cur_per_clock;

    if (target->state != TARGET_HALTED) {
        LOG_ERROR("Target not halted");
        return ERROR_TARGET_NOT_HALTED;
    }

    retval = target_write_u32(target, FLASH_KEY, KEY);
    if (retval != ERROR_OK)
        goto lock;

    retval = target_read_u32(target, FLASH_CMD, &flash_cmd);
    if (retval != ERROR_OK)
        goto lock;

    if ((first == 0) && (last == (bank->num_sectors - 1)) && !mdr149_info->mem_type) {
        retval = mdr149_mass_erase(bank);
        goto lock;
    }

    LOG_INFO("MDR149: ERASE pages from 0x%" PRIx32 " to 0x%" PRIx32 "", first, last);

    /* Switch on register access */
    flash_cmd = (flash_cmd & (FLASH_CNTR_WAIT_Msk | FLASH_CNTR_ERASE_START_Msk)) |
                FLASH_CNTR_MODE_Msk | FLASH_CNTR_TMR_Msk;
    if (mdr149_info->mem_type)
        flash_cmd |= FLASH_CNTR_IFREN_Msk;
    retval = target_write_u32(target, FLASH_CMD, flash_cmd);
    if (retval != ERROR_OK)
        goto reset_pg_and_lock;

    unsigned int page_size = bank->size / mdr149_info->page_count;
    LOG_DEBUG("MDR149: page_size 0x%" PRIx32 " page_count 0x%" PRIx32 " lock_code 0x%" PRIx32 "",
              page_size, mdr149_info->page_count, mdr149_info->lock_code);

    for (unsigned int i = first; i <= last; i++) {
        uint32_t adr = i * page_size; // adr must be divisible by 0x8000

        flash_cmd |= FLASH_CNTR_MODE_Msk;
        flash_cmd |= FLASH_CNTR_TMR_Msk;
        flash_cmd &= ~FLASH_CNTR_SE50_Msk; //SE[5:0]=0
        flash_cmd &= ~(FLASH_CNTR_NVSTR_Msk | FLASH_CNTR_PROG_Msk | FLASH_CNTR_MAS1_Msk |
                       FLASH_CNTR_ERASE_Msk); //NVSTR=0, PROG=0, MAS1=0,ERASE=0
        //flash_cmd |= MEMORY<<21;//IFREN=MEMORY
        UPD(flash_cmd);

        // Erase Page0
        flash_cmd |= FLASH_CNTR_FLASH30_CR_Msk; //SEL[3:0]=1
        retval = target_write_u32(target, FLASH_ADR, adr >> 4);
        if (retval != ERROR_OK)
            goto reset_pg_and_lock;

        flash_cmd |= FLASH_CNTR_XE_Msk; //XE=1
        UPD(flash_cmd);
        flash_cmd |= FLASH_CNTR_ERASE_Msk; //ERASE=1
        UPD(flash_cmd);
        usleep(5); /* Wait for 5 us */
        flash_cmd |= FLASH_CNTR_NVSTR_Msk; //NVSTR = 1
        UPD(flash_cmd);
        alive_sleep(40); /* Wait for 40000 us */
        flash_cmd &= ~(FLASH_CNTR_ERASE_Msk); //ERASE=0
        UPD(flash_cmd);
        usleep(5); /* Wait for 5 us */
        flash_cmd &= ~(FLASH_CNTR_NVSTR_Msk); //NVSTR = 0
        UPD(flash_cmd);
        flash_cmd &= ~(FLASH_CNTR_XE_Msk); //XE=0
        UPD(flash_cmd);
        usleep(10); /* Wait for 10 us */
        flash_cmd &= ~(FLASH_CNTR_FLASH30_CR_Msk); //SEL[3:0]=0
        UPD(flash_cmd);

        // Erase Page1
        flash_cmd |= FLASH_CNTR_FLASH30_CR_Msk; //SEL[3:0]=1
        retval = target_write_u32(target, FLASH_ADR, (adr + 0x4000) >> 4);
        if (retval != ERROR_OK)
            goto reset_pg_and_lock;
        flash_cmd |= FLASH_CNTR_XE_Msk; //XE=1
        UPD(flash_cmd);
        flash_cmd |= FLASH_CNTR_ERASE_Msk; //ERASE=1
        UPD(flash_cmd);
        usleep(5); /* Wait for 5 us */
        flash_cmd |= FLASH_CNTR_NVSTR_Msk; //NVSTR = 1
        UPD(flash_cmd);
        alive_sleep(40); /* Wait for 40000 us */
        flash_cmd &= ~(FLASH_CNTR_ERASE_Msk); //ERASE=0
        UPD(flash_cmd);
        usleep(5); /* Wait for 5 us */
        flash_cmd &= ~(FLASH_CNTR_NVSTR_Msk); //NVSTR = 0
        UPD(flash_cmd);
        flash_cmd &= ~(FLASH_CNTR_XE_Msk); //XE=0
        UPD(flash_cmd);
        usleep(10); /* Wait for 10 us */
        flash_cmd &= ~(FLASH_CNTR_FLASH30_CR_Msk); //SEL[3:0]=0

        // Erase ECC Page
        flash_cmd |= FLASH_CNTR_FLASH54_CR_Msk; //SEL[5:4]=1
        retval = target_write_u32(target, FLASH_ADR, (adr + 0x0000) >> 5);
        if (retval != ERROR_OK)
            goto reset_pg_and_lock;
        flash_cmd |= FLASH_CNTR_XE_Msk; //XE=1
        UPD(flash_cmd);
        flash_cmd |= FLASH_CNTR_ERASE_Msk; //ERASE=1
        UPD(flash_cmd);
        usleep(5); /* Wait for 5 us */
        flash_cmd |= FLASH_CNTR_NVSTR_Msk; //NVSTR = 1
        UPD(flash_cmd);
        alive_sleep(40); /* Wait for 40000 us */
        flash_cmd &= ~(FLASH_CNTR_ERASE_Msk); //ERASE=0
        UPD(flash_cmd);
        usleep(5); /* Wait for 5 us */
        flash_cmd &= ~(FLASH_CNTR_NVSTR_Msk); //NVSTR = 0
        UPD(flash_cmd);
        flash_cmd &= ~(FLASH_CNTR_XE_Msk); //XE=0
        usleep(10); /* Wait for 10 us */
        flash_cmd &= ~(FLASH_CNTR_FLASH54_CR_Msk); //SEL[5:4]=0
        UPD(flash_cmd);

        flash_cmd &= ~(FLASH_CNTR_IFREN_Msk); //IFREN=0
        flash_cmd &= ~FLASH_CNTR_TMR_Msk;
        flash_cmd &= ~FLASH_CNTR_MODE_Msk;
        UPD(flash_cmd);

        bank->sectors[i].is_erased = 1;
    }

reset_pg_and_lock:
    flash_cmd &= (FLASH_CNTR_WAIT_Msk | FLASH_CNTR_ERASE_START_Msk);
    retval2 = target_write_u32(target, FLASH_CMD, flash_cmd);
    if (retval == ERROR_OK)
        retval = retval2;
lock:
    retval2 = target_write_u32(target, FLASH_KEY, 0x0);
    if (retval == ERROR_OK)
        retval = retval2;

    return retval;
}

static int mdr149_write_block(struct flash_bank *bank, const uint8_t *buffer, uint32_t offset,
                              uint32_t count)
{
    struct target *target = bank->target;
    struct mdr149_flash_bank *mdr149_info = bank->driver_priv;
    unsigned int page_size = bank->size / mdr149_info->page_count;
    uint32_t buffer_size = page_size;
    struct working_area *write_algorithm;
    struct working_area *source;
    const uint32_t num_reg_params = 4;
    uint32_t address = bank->base + offset;
    struct reg_param reg_params[num_reg_params];
    struct armv7m_algorithm armv7m_info;
    int retval = ERROR_OK;

    /* flash write code */
    LOG_DEBUG("MDR149: request  0x%" PRIx32 " bytes", (unsigned int)sizeof(mdr149_code));
    if (target_alloc_working_area(target, sizeof(mdr149_code), &write_algorithm) != ERROR_OK) {
        LOG_WARNING("no working area available, can't do block memory writes");
        return ERROR_TARGET_RESOURCE_NOT_AVAILABLE;
    }

    retval =
            target_write_buffer(target, write_algorithm->address, sizeof(mdr149_code), mdr149_code);
    if (retval != ERROR_OK)
        return retval;

    /* memory buffer */
    LOG_DEBUG("MDR149: request  0x%" PRIx32 " bytes", buffer_size);
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
    init_reg_param(&reg_params[0], "r0", 32, PARAM_IN_OUT);
    init_reg_param(&reg_params[1], "r1", 32, PARAM_OUT);
    init_reg_param(&reg_params[2], "r2", 32, PARAM_OUT);
    init_reg_param(&reg_params[3], "r3", 32, PARAM_OUT);
    //init_reg_param(&reg_params[4], "r4", 32, PARAM_IN_OUT);
    while (count > 0) {
        unsigned int bytes_to_write =
                buffer_size < count ? buffer_size : count; //min(buffer_size, count);
        retval = target_write_buffer(target, source->address, bytes_to_write, buffer);
        if (retval != ERROR_OK)
            goto free_buffer;

        buf_set_u32(reg_params[0].value, 0, 32, address);
        buf_set_u32(reg_params[1].value, 0, 32, bytes_to_write);
        buf_set_u32(reg_params[2].value, 0, 32, source->address);
        buf_set_u32(reg_params[3].value, 0, 32, mdr149_info->mem_type);
        //        buf_set_u32(reg_params[4].value, 0, 32, );

        armv7m_info.common_magic = ARMV7M_COMMON_MAGIC;
        armv7m_info.core_mode = ARM_MODE_THREAD;
        LOG_DEBUG("MDR149: adr      0x%" PRIx32 "", buf_get_u32(reg_params[0].value, 0, 32));
        LOG_DEBUG("MDR149: sz       0x%" PRIx32 "", buf_get_u32(reg_params[1].value, 0, 32));
        LOG_DEBUG("MDR149: buf      0x%" PRIx32 "", buf_get_u32(reg_params[2].value, 0, 32));
        LOG_DEBUG("MDR149: mem_type 0x%" PRIx32 "", buf_get_u32(reg_params[3].value, 0, 32));
        //        LOG_DEBUG("MDR149: address        0x%"PRIx32"", buf_get_u32(reg_params[4].value, 0, 32));

        //    retval = target_run_flash_async_algorithm(target,
        //          buffer, count, 4, // *buffer, count, block_size,
        //            0, NULL, // num_mem_params,  *mem_params,
        //            num_reg_params, reg_params, // num_reg_params,  *reg_params,
        //            source->address, source->size, // buffer_start, buffer_size
        //            write_algorithm->address+0x11D, 0, // entry_point, exit_point
        //            &armv7m_info); // *arch_info

        //int target_run_algorithm(struct target *target,
        //      int num_mem_params, struct mem_param *mem_params,
        //      int num_reg_params, struct reg_param *reg_param,
        //      uint32_t entry_point, uint32_t exit_point,
        //      int timeout_ms, void *arch_info)

        retval = target_run_algorithm(target, 0, NULL, num_reg_params, reg_params,
                                      write_algorithm->address + 0x11D, 0, 1000, &armv7m_info);

        LOG_DEBUG("MDR149: status      0x%" PRIx32 "", buf_get_u32(reg_params[0].value, 0, 32));
        //        LOG_DEBUG("MDR149: word_count  0x%"PRIx32"", buf_get_u32(reg_params[1].value, 0, 32));
        //        LOG_DEBUG("MDR149: start       0x%"PRIx32"", buf_get_u32(reg_params[2].value, 0, 32));
        //        LOG_DEBUG("MDR149: end         0x%"PRIx32"", buf_get_u32(reg_params[3].value, 0, 32));
        //        LOG_DEBUG("MDR149: address     0x%"PRIx32"", buf_get_u32(reg_params[4].value, 0, 32));

        if (retval == ERROR_FLASH_OPERATION_FAILED) {
            //            LOG_ERROR("flash write failed at address 0x%"PRIx32, buf_get_u32(reg_params[4].value, 0, 32));
            LOG_ERROR("MDR149: flash write failed");
            break;
        }
        address += bytes_to_write;
        count -= bytes_to_write;
        buffer += bytes_to_write;
    } // while
free_buffer:
    target_free_working_area(target, source);
    target_free_working_area(target, write_algorithm);

    for (unsigned int i = 0; i < num_reg_params; i++)
        destroy_reg_param(&reg_params[i]);

    return retval;
}

static int mdr149_write(struct flash_bank *bank, const uint8_t *buffer, uint32_t offset,
                        uint32_t count)
{
    struct target *target = bank->target;
    struct mdr149_flash_bank *mdr149_info = bank->driver_priv;
    uint8_t *new_buffer = NULL;

    if (target->state != TARGET_HALTED) {
        LOG_ERROR("Target not halted");
        return ERROR_TARGET_NOT_HALTED;
    }

    if (offset % 32) {
        LOG_ERROR("offset 0x%" PRIx32 " breaks required 32-byte alignment", offset);
        return ERROR_FLASH_DST_BREAKS_ALIGNMENT;
    }

    /* If there's an odd number of bytes, the data has to be padded. Duplicate
     * the buffer and use the normal code path with a single block write since
     * it's probably cheaper than to special case the last odd write using
     * discrete accesses. */
    int rem = (-count) % 32;
    if (rem) {
        new_buffer = malloc(count + rem);
        if (new_buffer == NULL) {
            LOG_ERROR("No memory for padding buffer");
            return ERROR_FAIL;
        }
        LOG_INFO("Non-multiple of 32 number of bytes to write, padding with 0xff");
        buffer = memcpy(new_buffer, buffer, count);
        while (rem--)
            new_buffer[count++] = 0xff;
    }

    uint32_t flash_cmd; //, cur_per_clock;
    int retval, retval2;

    retval = target_write_u32(target, FLASH_KEY, KEY);
    if (retval != ERROR_OK)
        goto free_buffer;

    retval = target_read_u32(target, FLASH_CMD, &flash_cmd);
    if (retval != ERROR_OK)
        goto lock;

    /* Switch on register access */
    flash_cmd = (flash_cmd & (FLASH_CNTR_WAIT_Msk | FLASH_CNTR_ERASE_START_Msk)) |
                FLASH_CNTR_MODE_Msk | FLASH_CNTR_TMR_Msk;
    if (mdr149_info->mem_type)
        flash_cmd |= FLASH_CNTR_IFREN_Msk;
    retval = target_write_u32(target, FLASH_CMD, flash_cmd);
    if (retval != ERROR_OK)
        goto reset_pg_and_lock;

    unsigned int page_size = bank->size / mdr149_info->page_count;
    LOG_INFO("MDR149: PROGRAM 0x%" PRIx32 " bytes at offset 0x%" PRIx32 "", count, offset);
    LOG_DEBUG("MDR149: bank_size 0x%" PRIx32 " page_size 0x%" PRIx32 " page_count 0x%" PRIx32
              " lock_code 0x%" PRIx32 "",
              bank->size, page_size, mdr149_info->page_count, mdr149_info->lock_code);
    /* try using block write */
    retval = mdr149_write_block(bank, buffer, offset, count);

    if (retval == ERROR_TARGET_RESOURCE_NOT_AVAILABLE) {
        /* if block write failed (no sufficient working area),
         * we use normal (slow) single halfword accesses */
        LOG_WARNING("Can't use block writes, falling back to single memory accesses");

        while (count > 0) {
            flash_cmd |= FLASH_CNTR_MODE_Msk;
            flash_cmd |= FLASH_CNTR_TMR_Msk;
            flash_cmd &= ~FLASH_CNTR_SE50_Msk; //SE[5:0]=0
            flash_cmd &= ~(FLASH_CNTR_NVSTR_Msk | FLASH_CNTR_PROG_Msk | FLASH_CNTR_MAS1_Msk |
                           FLASH_CNTR_ERASE_Msk); //NVSTR=0, PROG=0, MAS1=0,ERASE=0
            //flash_cmd |= MEMORY<<21;//IFREN=MEMORY
            flash_cmd |= FLASH_CNTR_FLASH30_CR_Msk; //SEL[3:0]=1
            UPD(flash_cmd);
            retval = target_write_u32(target, FLASH_WDATA0, M32(buffer + 0));
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;
            retval = target_write_u32(target, FLASH_WDATA1, M32(buffer + 4));
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;
            retval = target_write_u32(target, FLASH_WDATA2, M32(buffer + 8));
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;
            retval = target_write_u32(target, FLASH_WDATA3, M32(buffer + 12));
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;

            retval = target_write_u32(target, FLASH_ADR, offset >> 4);
            if (retval != ERROR_OK)
                goto reset_pg_and_lock;

            flash_cmd |= FLASH_CNTR_XE_Msk; //XE=1
            UPD(flash_cmd); // -10 ns
            flash_cmd |= FLASH_CNTR_PROG_Msk; //PROG=1
            UPD(flash_cmd);
            usleep(5); /* Wait for 5 us */
            flash_cmd |= FLASH_CNTR_NVSTR_Msk; //NVSTR = 1
            UPD(flash_cmd);
            usleep(10); /* Wait for 10 us */
            flash_cmd |= FLASH_CNTR_YE_Msk; //YE=1
            UPD(flash_cmd);
            usleep(20); /* Wait for 20 us */
            flash_cmd &= ~(FLASH_CNTR_YE_Msk); //YE=0
            UPD(flash_cmd); // 20 ns
            flash_cmd &= ~(FLASH_CNTR_PROG_Msk); //PROG=0
            UPD(flash_cmd);
            usleep(5); /* Wait for 5 us */
            flash_cmd &= ~(FLASH_CNTR_NVSTR_Msk); //NVSTR = 0
            UPD(flash_cmd); // -10 ns
            flash_cmd &= ~(FLASH_CNTR_XE_Msk); //XE=0
            UPD(flash_cmd);
            usleep(10); /* Wait for 10 us */
            flash_cmd &= ~(FLASH_CNTR_FLASH30_CR_Msk); //SEL[3:0]=0
            UPD(flash_cmd);

            if ((offset & 0x10) == 0x00) {
                uint32_t adr = bank->base + offset;
                uint32_t e0 = (ecc(adr + 12, M32(buffer + 12)) << 24) |
                              (ecc(adr +  8, M32(buffer +  8)) << 16) |
                              (ecc(adr +  4, M32(buffer +  4)) <<  8) |
                              (ecc(adr +  0, M32(buffer +  0)) <<  0);
                uint32_t e1 = (ecc(adr + 28, M32(buffer + 28)) << 24) |
                              (ecc(adr + 24, M32(buffer + 24)) << 16) |
                              (ecc(adr + 20, M32(buffer + 20)) <<  8) |
                              (ecc(adr + 16, M32(buffer + 16)) <<  0);

                keep_alive();

                //write ecc

                retval = target_write_u32(target, FLASH_ADR, offset >> 5);
                if (retval != ERROR_OK)
                    goto reset_pg_and_lock;

                retval = target_write_u32(target, FLASH_WECC0, e0);
                if (retval != ERROR_OK)
                    goto reset_pg_and_lock;

                retval = target_write_u32(target, FLASH_WECC1, e1);
                if (retval != ERROR_OK)
                    goto reset_pg_and_lock;

                flash_cmd |= FLASH_CNTR_FLASH54_CR_Msk; //SEL[5:4]=1
                flash_cmd |= FLASH_CNTR_XE_Msk; //XE=1
                UPD(flash_cmd); // -10 ns
                flash_cmd |= FLASH_CNTR_PROG_Msk; //PROG=1
                UPD(flash_cmd);
                usleep(5); /* Wait for 5 us */
                UPD(flash_cmd);
                flash_cmd |= FLASH_CNTR_NVSTR_Msk; //NVSTR = 1
                UPD(flash_cmd);
                usleep(10); /* Wait for 10 us */
                flash_cmd |= FLASH_CNTR_YE_Msk; //YE=1
                UPD(flash_cmd);
                usleep(20); /* Wait for 20 us */
                flash_cmd &= ~(FLASH_CNTR_YE_Msk); //YE=0
                UPD(flash_cmd); // 20 ns
                flash_cmd &= ~(FLASH_CNTR_PROG_Msk); //PROG=0
                UPD(flash_cmd);
                usleep(5); /* Wait for 5 us */
                flash_cmd &= ~(FLASH_CNTR_NVSTR_Msk); //NVSTR = 0
                UPD(flash_cmd); // -10 ns
                flash_cmd &= ~(FLASH_CNTR_XE_Msk); //XE=0
                UPD(flash_cmd);
                usleep(10); /* Wait for 10 us */
                flash_cmd &= ~(FLASH_CNTR_FLASH54_CR_Msk); //SEL[5:4]=0
                //flash_cmd &= ~(FLASH_CNTR_IFREN_Msk);//IFREN=0
                flash_cmd &= ~FLASH_CNTR_TMR_Msk;
                flash_cmd &= ~FLASH_CNTR_MODE_Msk;
                UPD(flash_cmd);
            }
            // Go to next
            offset += 16;
            buffer += 16;
            count -= 16;
        } // while
    } // ERROR_TARGET_RESOURCE_NOT_AVAILABLE

reset_pg_and_lock:
    flash_cmd &= (FLASH_CNTR_WAIT_Msk | FLASH_CNTR_ERASE_START_Msk);
    retval2 = target_write_u32(target, FLASH_CMD, flash_cmd);
    if (retval == ERROR_OK)
        retval = retval2;
lock:
    retval2 = target_write_u32(target, FLASH_KEY, 0);
    if (retval == ERROR_OK)
        retval = retval2;

free_buffer:
    //if (new_buffer)
    free(new_buffer);

    //    /* read some bytes bytes to flush buffer in flash accelerator.
    //     * See errata for 1986VE1T and 1986VE3. Error 0007 */
    //    if ((retval == ERROR_OK) && (!mdr149_info->mem_type)) {
    //        uint32_t tmp;
    //        target_checksum_memory(bank->target, bank->base, 64, &tmp);
    //    }

    return retval;
}

static int mdr149_read(struct flash_bank *bank, uint8_t *buffer, uint32_t offset, uint32_t count)
{
    struct target *target = bank->target;
    struct mdr149_flash_bank *mdr149_info = bank->driver_priv;
    int retval, retval2;

    LOG_DEBUG("MDR149: offset 0x%" PRIx32 " count 0x%" PRIx32 "", offset, count);

    if (!mdr149_info->mem_type) //!
        return default_flash_read(bank, buffer, offset, count);

    if (target->state != TARGET_HALTED) {
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

    uint32_t flash_cmd; //, cur_per_clock;

    //  retval = target_read_u32(target, MD_PER_CLOCK, &cur_per_clock);
    //  if (retval != ERROR_OK)
    //      goto err;

    //  if (!(cur_per_clock & MD_PER_CLOCK_RST_CLK)) {
    //      /* Something's very wrong if the RST_CLK module is not clocked */
    //      LOG_ERROR("Target needs reset before flash operations");
    //      retval = ERROR_FLASH_OPERATION_FAILED;
    //      goto err;
    //  }

    //  retval = target_write_u32(target, MD_PER_CLOCK, cur_per_clock | MD_PER_CLOCK_EEPROM);
    //  if (retval != ERROR_OK)
    //      goto err;

    retval = target_write_u32(target, FLASH_KEY, KEY);
    if (retval != ERROR_OK)
        goto err;

    retval = target_read_u32(target, FLASH_CMD, &flash_cmd);
    if (retval != ERROR_OK)
        goto err_lock;

    /* Switch on register access */
    flash_cmd = (flash_cmd & (FLASH_CNTR_WAIT_Msk | FLASH_CNTR_ERASE_START_Msk)) |
                FLASH_CNTR_MODE_Msk | FLASH_CNTR_TMR_Msk;
    flash_cmd |= FLASH_CNTR_IFREN_Msk;
    retval = target_write_u32(target, FLASH_CMD, flash_cmd);
    if (retval != ERROR_OK)
        goto reset_pg_and_lock;

    for (uint32_t i = 0; i < count; i += 4) {
        retval = target_write_u32(target, FLASH_ADR, offset + i);
        if (retval != ERROR_OK)
            goto reset_pg_and_lock;

        retval = target_write_u32(target, FLASH_CMD,
                                  flash_cmd | FLASH_CNTR_XE_Msk | FLASH_CNTR_YE_Msk |
                                          FLASH_CNTR_SE30_Msk); //  | FLASH_CNTR_SE50_Msk
        if (retval != ERROR_OK)
            goto reset_pg_and_lock;

        //        uint32_t buf;
        //        retval = target_read_u32(target, FLASH_DO, &buf);
        //        if (retval != ERROR_OK)
        //            goto reset_pg_and_lock;

        //        buf_set_u32(buffer, i * 8, 32, buf);

        retval = target_write_u32(target, FLASH_CMD, flash_cmd);
        if (retval != ERROR_OK)
            goto reset_pg_and_lock;
    }

reset_pg_and_lock:
    flash_cmd &= FLASH_CNTR_WAIT_Msk;
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

static int mdr149_probe(struct flash_bank *bank)
{
    struct mdr149_flash_bank *mdr149_info = bank->driver_priv;
    unsigned int page_count, page_size, i;

    page_count = mdr149_info->page_count;
    page_size = bank->size / page_count;

    //if (bank->sectors) {
    free(bank->sectors);
    //  bank->sectors = NULL;
    //}

    bank->num_sectors = page_count;
    bank->sectors = malloc(sizeof(struct flash_sector) * bank->num_sectors);

    for (i = 0; i < bank->num_sectors; i++) {
        bank->sectors[i].offset = i * page_size;
        bank->sectors[i].size = page_size;
        bank->sectors[i].is_erased = -1;
        bank->sectors[i].is_protected = -1;
    }

    bank->num_prot_blocks = 1;
    bank->prot_blocks = malloc(sizeof(struct flash_sector) * bank->num_prot_blocks);

    for (i = 0; i < bank->num_prot_blocks; i++) {
        bank->prot_blocks[i].offset = 0xE0000;
        bank->prot_blocks[i].size = 0x20000;
        bank->prot_blocks[i].is_erased = -1;
        bank->prot_blocks[i].is_protected = -1;
    }

    mdr149_info->probed = true;

    return ERROR_OK;
}

static int mdr149_auto_probe(struct flash_bank *bank)
{
    struct mdr149_flash_bank *mdr149_info = bank->driver_priv;
    if (mdr149_info->probed)
        return ERROR_OK;
    return mdr149_probe(bank);
}


int get_mdr149_info(struct flash_bank *bank, struct command_invocation *cmd)
{
    struct mdr149_flash_bank *mdr149_info = bank->driver_priv;
    command_print_sameline(cmd, "MDR149 - %s", mdr149_info->mem_type ? "info memory" : "main memory");

    struct target *target = bank->target;
    uint32_t flash_cmd;
    int retval;

    retval = target_read_u32(target, FLASH_CMD, &flash_cmd);
    if (retval == ERROR_OK) {
        uint32_t BRKTHRU_DONE =
                (flash_cmd & FLASH_CNTR_BRKTHRU_DONE_Msk) >> FLASH_CNTR_ERASE_DONE_Pos;
        uint32_t ERASE_DONE = (flash_cmd & FLASH_CNTR_ERASE_DONE_Msk) >> FLASH_CNTR_ERASE_DONE_Pos;
        uint32_t breakthrough =
                (flash_cmd & FLASH_CNTR_breakthrough_Msk) >> FLASH_CNTR_breakthrough_Pos;
        LOG_DEBUG("MDR149: FLASH_CMD 0x%" PRIx32 " ", flash_cmd);
        command_print_sameline(cmd,
                 "MDR149 - %s BRKTHRU_DONE=%" PRIx32 " ERASE_DONE=%" PRIx32 " breakthrough=%" PRIx32
                 "",
                 mdr149_info->mem_type ? "info memory" : "main memory", BRKTHRU_DONE, ERASE_DONE,
                 breakthrough);
    }
    return ERROR_OK;
}

int get_mdr149_info_OLDAPI(struct flash_bank *bank, char *buf, int buf_size)
{
    struct mdr149_flash_bank *mdr149_info = bank->driver_priv;
    snprintf(buf, buf_size, "MDR149 - %s", mdr149_info->mem_type ? "info memory" : "main memory");

    struct target *target = bank->target;
    uint32_t flash_cmd;
    int retval;

    retval = target_read_u32(target, FLASH_CMD, &flash_cmd);
    if (retval == ERROR_OK) {
        uint32_t BRKTHRU_DONE =
                (flash_cmd & FLASH_CNTR_BRKTHRU_DONE_Msk) >> FLASH_CNTR_ERASE_DONE_Pos;
        uint32_t ERASE_DONE = (flash_cmd & FLASH_CNTR_ERASE_DONE_Msk) >> FLASH_CNTR_ERASE_DONE_Pos;
        uint32_t breakthrough =
                (flash_cmd & FLASH_CNTR_breakthrough_Msk) >> FLASH_CNTR_breakthrough_Pos;
        LOG_DEBUG("MDR149: FLASH_CMD 0x%" PRIx32 " ", flash_cmd);
        snprintf(buf, buf_size,
                 "MDR149 - %s BRKTHRU_DONE=%" PRIx32 " ERASE_DONE=%" PRIx32 " breakthrough=%" PRIx32
                 "",
                 mdr149_info->mem_type ? "info memory" : "main memory", BRKTHRU_DONE, ERASE_DONE,
                 breakthrough);
    }
    return ERROR_OK;
}

static int mdr149_protect_check(struct flash_bank *bank)
{
    struct target *target = bank->target;
    uint32_t flash_cmd;
    int retval;

    LOG_DEBUG("mdr149_protect_check");

    if (target->state != TARGET_HALTED) {
        LOG_ERROR("Target not halted");
        return ERROR_TARGET_NOT_HALTED;
    }

    retval = target_read_u32(target, FLASH_CMD, &flash_cmd);
    if (retval != ERROR_OK)
        return retval;

    //uint32_t  ERASE_DONE   = (flash_cmd & FLASH_CNTR_ERASE_DONE_Msk) >> FLASH_CNTR_ERASE_DONE_Pos;
    uint32_t breakthrough =
            (flash_cmd & FLASH_CNTR_breakthrough_Msk) >> FLASH_CNTR_breakthrough_Pos;
    LOG_DEBUG("MDR149: FLASH_CMD 0x%" PRIx32 "", flash_cmd);

    //for (i = 0; i < bank->num_prot_blocks; i++) {
    //    bank->prot_blocks[i].is_protected = (breakthrough != 0);
    //}
    bank->prot_blocks[0].is_protected = (breakthrough == 0);

    LOG_DEBUG("Done");
    return ERROR_OK;
}

static int mdr149_protect(struct flash_bank *bank, int set, unsigned int first, unsigned int last)
{
    struct target *target = bank->target;
    struct mdr149_flash_bank *mdr149_info = bank->driver_priv;
    uint32_t flash_cmd;
    int retval;

    LOG_DEBUG("mdr149_protect %" PRIx32 " %" PRIx32 " %" PRIx32 " ", set, first, last);

    if (target->state != TARGET_HALTED) {
        LOG_ERROR("Target not halted");
        return ERROR_TARGET_NOT_HALTED;
    }

    retval = target_write_u32(target, FLASH_KEY, KEY);
    if (retval != ERROR_OK)
        goto lock;

    retval = target_read_u32(target, FLASH_CMD, &flash_cmd);
    if (retval != ERROR_OK)
        goto lock;

    if (set) {
        FlashLock(bank, mdr149_info->lock_code); // 0x12345678

        #if 1
        // reset ERASE_START & ERASE_DONE bit
        retval = target_write_u32(target, FLASH_KEY, KEY);
        if (retval != ERROR_OK)
            goto lock;

        flash_cmd &= ~(FLASH_CNTR_ERASE_START_Msk);
        retval = target_write_u32(target, FLASH_CMD, flash_cmd);
        if (retval != ERROR_OK)
            goto reset_pg_and_lock;
        #endif
    } else {
        flash_cmd &= (FLASH_CNTR_WAIT_Msk |
                      FLASH_CNTR_ERASE_START_Msk); //  | FLASH_CNTR_BRKTHRU_DONE_Msk
        retval = target_write_u32(target, FLASH_CMD, flash_cmd);
        if (retval != ERROR_OK)
            goto reset_pg_and_lock;

        retval = target_write_u32(target, FLASH_BLOCK, 0xB3C3B3C3);
        if (retval != ERROR_OK)
            goto reset_pg_and_lock;

        flash_cmd |= FLASH_CNTR_BRKTHRU_START_Msk;
        retval = target_write_u32(target, FLASH_CMD, flash_cmd);
        if (retval != ERROR_OK)
            goto reset_pg_and_lock;

        int i = 0;
        do {
            alive_sleep(1); // ms
            if (++i == 10) {
                retval = 1;
                goto reset_pg_and_lock;
            }
            retval = target_read_u32(target, FLASH_CMD, &flash_cmd);
            if (retval != ERROR_OK)
                continue;

        } while ((flash_cmd & FLASH_CNTR_BRKTHRU_DONE_Msk) != FLASH_CNTR_BRKTHRU_DONE_Msk);
        uint32_t breakthrough =
                (flash_cmd & FLASH_CNTR_breakthrough_Msk) >> FLASH_CNTR_breakthrough_Pos;
        if (breakthrough == 0) {
            LOG_INFO("MDR149: Breakthrough failed");

            retval = 2;
            goto reset_pg_and_lock;
        }
    } // if (set)

    int retval2;
reset_pg_and_lock:
    flash_cmd &= (FLASH_CNTR_WAIT_Msk | FLASH_CNTR_ERASE_START_Msk | FLASH_CNTR_BRKTHRU_DONE_Msk);
    retval2 = target_write_u32(target, FLASH_CMD, flash_cmd);
    if (retval == ERROR_OK)
        retval = retval2;
lock:
    retval2 = target_write_u32(target, FLASH_KEY, 0x0);
    if (retval == ERROR_OK)
        retval = retval2;

    return retval;
}

const struct flash_driver mdr149_flash = {
    .name = "mdr149",
    .usage = "flash bank <name> mdr149 <base> <size> 0 0 <target#> <type> <page_count> <lock_code>"
             "<type>: 0 for main memory, 1 for info memory"
             "<lock_code>: 0x12345678 for OPEN, 0xABCDEF12 for PROT1, 0x562C17D4 for PROT2",
    .flash_bank_command = mdr149_flash_bank_command,
    .erase = mdr149_erase,
    .write = mdr149_write,
    .read = mdr149_read,
    .probe = mdr149_probe,
    .auto_probe = mdr149_auto_probe,
    .erase_check = default_flash_blank_check,
    .info = __builtin_choose_expr (__builtin_types_compatible_p(typeof(&get_mdr149_info), typeof(mdr149_flash.info)), get_mdr149_info, get_mdr149_info_OLDAPI),
    .free_driver_priv = default_flash_free_driver_priv,

    .protect = mdr149_protect,
    .protect_check = mdr149_protect_check,

};

/*
openocd -f interface/cmsis-dap.cfg -f target/mdr149.cfg
openocd -f interface/cmsis-dap.cfg -c "transport select jtag" -f target/mdr149.cfg
openocd -f interface/cmsis-dap.cfg -c "set LOCK_CODE 0xABCDEF12" -f target/mdr149.cfg

## program and set protection example
reset halt
#after reset flash in "protected" state
flash info mdr149
#full chip erase to unprotect
flash erase_sector mdr149 0 last
#now we can see "not protected"
flash info mdr149
#flash example (optional)
flash write_image unlock_Flash.axf
#verify example (optional)
flash verify_image unlock_Flash.axf
#set choosen mode
flash protect mdr149 0 0 "on"
#now we can see "protected"
flash info mdr149

## verify protection example
reset halt
#next command needed only for PROT2 mode
reg pc 0x01000000; step;
#after reset flash in "protected" state
flash info mdr149
#Try to unlock flash. If succeeded JTAG/SWD be disconnected in PROT1 PROT2 modes so OpenOCD hang.
flash protect mdr149 0 0 "off"
#in OPEN mode we can see "not protected"
flash info mdr149
*/
