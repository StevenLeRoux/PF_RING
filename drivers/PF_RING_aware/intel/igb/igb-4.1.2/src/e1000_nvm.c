/*******************************************************************************

  Intel(R) Gigabit Ethernet Linux driver
  Copyright(c) 2007-2012 Intel Corporation.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Contact Information:
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/

#include "e1000_api.h"

static void e1000_reload_nvm_generic(struct e1000_hw *hw);

/**
 *  e1000_init_nvm_ops_generic - Initialize NVM function pointers
 *  @hw: pointer to the HW structure
 *
 *  Setups up the function pointers to no-op functions
 **/
void e1000_init_nvm_ops_generic(struct e1000_hw *hw)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	DEBUGFUNC("e1000_init_nvm_ops_generic");

	/* Initialize function pointers */
	nvm->ops.init_params = e1000_null_ops_generic;
	nvm->ops.acquire = e1000_null_ops_generic;
	nvm->ops.read = e1000_null_read_nvm;
	nvm->ops.release = e1000_null_nvm_generic;
	nvm->ops.reload = e1000_reload_nvm_generic;
	nvm->ops.update = e1000_null_ops_generic;
	nvm->ops.valid_led_default = e1000_null_led_default;
	nvm->ops.validate = e1000_null_ops_generic;
	nvm->ops.write = e1000_null_write_nvm;
}

/**
 *  e1000_null_nvm_read - No-op function, return 0
 *  @hw: pointer to the HW structure
 **/
s32 e1000_null_read_nvm(struct e1000_hw *hw, u16 a, u16 b, u16 *c)
{
	DEBUGFUNC("e1000_null_read_nvm");
	return E1000_SUCCESS;
}

/**
 *  e1000_null_nvm_generic - No-op function, return void
 *  @hw: pointer to the HW structure
 **/
void e1000_null_nvm_generic(struct e1000_hw *hw)
{
	DEBUGFUNC("e1000_null_nvm_generic");
	return;
}

/**
 *  e1000_null_led_default - No-op function, return 0
 *  @hw: pointer to the HW structure
 **/
s32 e1000_null_led_default(struct e1000_hw *hw, u16 *data)
{
	DEBUGFUNC("e1000_null_led_default");
	return E1000_SUCCESS;
}

/**
 *  e1000_null_write_nvm - No-op function, return 0
 *  @hw: pointer to the HW structure
 **/
s32 e1000_null_write_nvm(struct e1000_hw *hw, u16 a, u16 b, u16 *c)
{
	DEBUGFUNC("e1000_null_write_nvm");
	return E1000_SUCCESS;
}

/**
 *  e1000_raise_eec_clk - Raise EEPROM clock
 *  @hw: pointer to the HW structure
 *  @eecd: pointer to the EEPROM
 *
 *  Enable/Raise the EEPROM clock bit.
 **/
static void e1000_raise_eec_clk(struct e1000_hw *hw, u32 *eecd)
{
	*eecd = *eecd | E1000_EECD_SK;
	E1000_WRITE_REG(hw, E1000_EECD, *eecd);
	E1000_WRITE_FLUSH(hw);
	usec_delay(hw->nvm.delay_usec);
}

/**
 *  e1000_lower_eec_clk - Lower EEPROM clock
 *  @hw: pointer to the HW structure
 *  @eecd: pointer to the EEPROM
 *
 *  Clear/Lower the EEPROM clock bit.
 **/
static void e1000_lower_eec_clk(struct e1000_hw *hw, u32 *eecd)
{
	*eecd = *eecd & ~E1000_EECD_SK;
	E1000_WRITE_REG(hw, E1000_EECD, *eecd);
	E1000_WRITE_FLUSH(hw);
	usec_delay(hw->nvm.delay_usec);
}

/**
 *  e1000_shift_out_eec_bits - Shift data bits our to the EEPROM
 *  @hw: pointer to the HW structure
 *  @data: data to send to the EEPROM
 *  @count: number of bits to shift out
 *
 *  We need to shift 'count' bits out to the EEPROM.  So, the value in the
 *  "data" parameter will be shifted out to the EEPROM one bit at a time.
 *  In order to do this, "data" must be broken down into bits.
 **/
static void e1000_shift_out_eec_bits(struct e1000_hw *hw, u16 data, u16 count)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	u32 eecd = E1000_READ_REG(hw, E1000_EECD);
	u32 mask;

	DEBUGFUNC("e1000_shift_out_eec_bits");

	mask = 0x01 << (count - 1);
	if (nvm->type == e1000_nvm_eeprom_spi)
		eecd |= E1000_EECD_DO;

	do {
		eecd &= ~E1000_EECD_DI;

		if (data & mask)
			eecd |= E1000_EECD_DI;

		E1000_WRITE_REG(hw, E1000_EECD, eecd);
		E1000_WRITE_FLUSH(hw);

		usec_delay(nvm->delay_usec);

		e1000_raise_eec_clk(hw, &eecd);
		e1000_lower_eec_clk(hw, &eecd);

		mask >>= 1;
	} while (mask);

	eecd &= ~E1000_EECD_DI;
	E1000_WRITE_REG(hw, E1000_EECD, eecd);
}

/**
 *  e1000_shift_in_eec_bits - Shift data bits in from the EEPROM
 *  @hw: pointer to the HW structure
 *  @count: number of bits to shift in
 *
 *  In order to read a register from the EEPROM, we need to shift 'count' bits
 *  in from the EEPROM.  Bits are "shifted in" by raising the clock input to
 *  the EEPROM (setting the SK bit), and then reading the value of the data out
 *  "DO" bit.  During this "shifting in" process the data in "DI" bit should
 *  always be clear.
 **/
static u16 e1000_shift_in_eec_bits(struct e1000_hw *hw, u16 count)
{
	u32 eecd;
	u32 i;
	u16 data;

	DEBUGFUNC("e1000_shift_in_eec_bits");

	eecd = E1000_READ_REG(hw, E1000_EECD);

	eecd &= ~(E1000_EECD_DO | E1000_EECD_DI);
	data = 0;

	for (i = 0; i < count; i++) {
		data <<= 1;
		e1000_raise_eec_clk(hw, &eecd);

		eecd = E1000_READ_REG(hw, E1000_EECD);

		eecd &= ~E1000_EECD_DI;
		if (eecd & E1000_EECD_DO)
			data |= 1;

		e1000_lower_eec_clk(hw, &eecd);
	}

	return data;
}

/**
 *  e1000_poll_eerd_eewr_done - Poll for EEPROM read/write completion
 *  @hw: pointer to the HW structure
 *  @ee_reg: EEPROM flag for polling
 *
 *  Polls the EEPROM status bit for either read or write completion based
 *  upon the value of 'ee_reg'.
 **/
s32 e1000_poll_eerd_eewr_done(struct e1000_hw *hw, int ee_reg)
{
	u32 attempts = 100000;
	u32 i, reg = 0;

	DEBUGFUNC("e1000_poll_eerd_eewr_done");

	for (i = 0; i < attempts; i++) {
		if (ee_reg == E1000_NVM_POLL_READ)
			reg = E1000_READ_REG(hw, E1000_EERD);
		else
			reg = E1000_READ_REG(hw, E1000_EEWR);

		if (reg & E1000_NVM_RW_REG_DONE)
			return E1000_SUCCESS;

		usec_delay(5);
	}

	return -E1000_ERR_NVM;
}

/**
 *  e1000_acquire_nvm_generic - Generic request for access to EEPROM
 *  @hw: pointer to the HW structure
 *
 *  Set the EEPROM access request bit and wait for EEPROM access grant bit.
 *  Return successful if access grant bit set, else clear the request for
 *  EEPROM access and return -E1000_ERR_NVM (-1).
 **/
s32 e1000_acquire_nvm_generic(struct e1000_hw *hw)
{
	u32 eecd = E1000_READ_REG(hw, E1000_EECD);
	s32 timeout = E1000_NVM_GRANT_ATTEMPTS;

	DEBUGFUNC("e1000_acquire_nvm_generic");

	E1000_WRITE_REG(hw, E1000_EECD, eecd | E1000_EECD_REQ);
	eecd = E1000_READ_REG(hw, E1000_EECD);

	while (timeout) {
		if (eecd & E1000_EECD_GNT)
			break;
		usec_delay(5);
		eecd = E1000_READ_REG(hw, E1000_EECD);
		timeout--;
	}

	if (!timeout) {
		eecd &= ~E1000_EECD_REQ;
		E1000_WRITE_REG(hw, E1000_EECD, eecd);
		DEBUGOUT("Could not acquire NVM grant\n");
		return -E1000_ERR_NVM;
	}

	return E1000_SUCCESS;
}

/**
 *  e1000_standby_nvm - Return EEPROM to standby state
 *  @hw: pointer to the HW structure
 *
 *  Return the EEPROM to a standby state.
 **/
static void e1000_standby_nvm(struct e1000_hw *hw)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	u32 eecd = E1000_READ_REG(hw, E1000_EECD);

	DEBUGFUNC("e1000_standby_nvm");

	if (nvm->type == e1000_nvm_eeprom_spi) {
		/* Toggle CS to flush commands */
		eecd |= E1000_EECD_CS;
		E1000_WRITE_REG(hw, E1000_EECD, eecd);
		E1000_WRITE_FLUSH(hw);
		usec_delay(nvm->delay_usec);
		eecd &= ~E1000_EECD_CS;
		E1000_WRITE_REG(hw, E1000_EECD, eecd);
		E1000_WRITE_FLUSH(hw);
		usec_delay(nvm->delay_usec);
	}
}

/**
 *  e1000_stop_nvm - Terminate EEPROM command
 *  @hw: pointer to the HW structure
 *
 *  Terminates the current command by inverting the EEPROM's chip select pin.
 **/
static void e1000_stop_nvm(struct e1000_hw *hw)
{
	u32 eecd;

	DEBUGFUNC("e1000_stop_nvm");

	eecd = E1000_READ_REG(hw, E1000_EECD);
	if (hw->nvm.type == e1000_nvm_eeprom_spi) {
		/* Pull CS high */
		eecd |= E1000_EECD_CS;
		e1000_lower_eec_clk(hw, &eecd);
	}
}

/**
 *  e1000_release_nvm_generic - Release exclusive access to EEPROM
 *  @hw: pointer to the HW structure
 *
 *  Stop any current commands to the EEPROM and clear the EEPROM request bit.
 **/
void e1000_release_nvm_generic(struct e1000_hw *hw)
{
	u32 eecd;

	DEBUGFUNC("e1000_release_nvm_generic");

	e1000_stop_nvm(hw);

	eecd = E1000_READ_REG(hw, E1000_EECD);
	eecd &= ~E1000_EECD_REQ;
	E1000_WRITE_REG(hw, E1000_EECD, eecd);
}

/**
 *  e1000_ready_nvm_eeprom - Prepares EEPROM for read/write
 *  @hw: pointer to the HW structure
 *
 *  Setups the EEPROM for reading and writing.
 **/
static s32 e1000_ready_nvm_eeprom(struct e1000_hw *hw)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	u32 eecd = E1000_READ_REG(hw, E1000_EECD);
	u8 spi_stat_reg;

	DEBUGFUNC("e1000_ready_nvm_eeprom");

	if (nvm->type == e1000_nvm_eeprom_spi) {
		u16 timeout = NVM_MAX_RETRY_SPI;

		/* Clear SK and CS */
		eecd &= ~(E1000_EECD_CS | E1000_EECD_SK);
		E1000_WRITE_REG(hw, E1000_EECD, eecd);
		E1000_WRITE_FLUSH(hw);
		usec_delay(1);

		/*
		 * Read "Status Register" repeatedly until the LSB is cleared.
		 * The EEPROM will signal that the command has been completed
		 * by clearing bit 0 of the internal status register.  If it's
		 * not cleared within 'timeout', then error out.
		 */
		while (timeout) {
			e1000_shift_out_eec_bits(hw, NVM_RDSR_OPCODE_SPI,
						 hw->nvm.opcode_bits);
			spi_stat_reg = (u8)e1000_shift_in_eec_bits(hw, 8);
			if (!(spi_stat_reg & NVM_STATUS_RDY_SPI))
				break;

			usec_delay(5);
			e1000_standby_nvm(hw);
			timeout--;
		}

		if (!timeout) {
			DEBUGOUT("SPI NVM Status error\n");
			return -E1000_ERR_NVM;
		}
	}

	return E1000_SUCCESS;
}

/**
 *  e1000_read_nvm_spi - Read EEPROM's using SPI
 *  @hw: pointer to the HW structure
 *  @offset: offset of word in the EEPROM to read
 *  @words: number of words to read
 *  @data: word read from the EEPROM
 *
 *  Reads a 16 bit word from the EEPROM.
 **/
s32 e1000_read_nvm_spi(struct e1000_hw *hw, u16 offset, u16 words, u16 *data)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	u32 i = 0;
	s32 ret_val;
	u16 word_in;
	u8 read_opcode = NVM_READ_OPCODE_SPI;

	DEBUGFUNC("e1000_read_nvm_spi");

	/*
	 * A check for invalid values:  offset too large, too many words,
	 * and not enough words.
	 */
	if ((offset >= nvm->word_size) || (words > (nvm->word_size - offset)) ||
	    (words == 0)) {
		DEBUGOUT("nvm parameter(s) out of bounds\n");
		return -E1000_ERR_NVM;
	}

	ret_val = nvm->ops.acquire(hw);
	if (ret_val)
		return ret_val;

	ret_val = e1000_ready_nvm_eeprom(hw);
	if (ret_val)
		goto release;

	e1000_standby_nvm(hw);

	if ((nvm->address_bits == 8) && (offset >= 128))
		read_opcode |= NVM_A8_OPCODE_SPI;

	/* Send the READ command (opcode + addr) */
	e1000_shift_out_eec_bits(hw, read_opcode, nvm->opcode_bits);
	e1000_shift_out_eec_bits(hw, (u16)(offset*2), nvm->address_bits);

	/*
	 * Read the data.  SPI NVMs increment the address with each byte
	 * read and will roll over if reading beyond the end.  This allows
	 * us to read the whole NVM from any offset
	 */
	for (i = 0; i < words; i++) {
		word_in = e1000_shift_in_eec_bits(hw, 16);
		data[i] = (word_in >> 8) | (word_in << 8);
	}

release:
	nvm->ops.release(hw);

	return ret_val;
}

/**
 *  e1000_read_nvm_eerd - Reads EEPROM using EERD register
 *  @hw: pointer to the HW structure
 *  @offset: offset of word in the EEPROM to read
 *  @words: number of words to read
 *  @data: word read from the EEPROM
 *
 *  Reads a 16 bit word from the EEPROM using the EERD register.
 **/
s32 e1000_read_nvm_eerd(struct e1000_hw *hw, u16 offset, u16 words, u16 *data)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	u32 i, eerd = 0;
	s32 ret_val = E1000_SUCCESS;

	DEBUGFUNC("e1000_read_nvm_eerd");

	/*
	 * A check for invalid values:  offset too large, too many words,
	 * too many words for the offset, and not enough words.
	 */
	if ((offset >= nvm->word_size) || (words > (nvm->word_size - offset)) ||
	    (words == 0)) {
		DEBUGOUT("nvm parameter(s) out of bounds\n");
		return -E1000_ERR_NVM;
	}

	for (i = 0; i < words; i++) {
		eerd = ((offset+i) << E1000_NVM_RW_ADDR_SHIFT) +
		       E1000_NVM_RW_REG_START;

		E1000_WRITE_REG(hw, E1000_EERD, eerd);
		ret_val = e1000_poll_eerd_eewr_done(hw, E1000_NVM_POLL_READ);
		if (ret_val)
			break;

		data[i] = (E1000_READ_REG(hw, E1000_EERD) >>
			   E1000_NVM_RW_REG_DATA);
	}

	return ret_val;
}

/**
 *  e1000_write_nvm_spi - Write to EEPROM using SPI
 *  @hw: pointer to the HW structure
 *  @offset: offset within the EEPROM to be written to
 *  @words: number of words to write
 *  @data: 16 bit word(s) to be written to the EEPROM
 *
 *  Writes data to EEPROM at offset using SPI interface.
 *
 *  If e1000_update_nvm_checksum is not called after this function , the
 *  EEPROM will most likely contain an invalid checksum.
 **/
s32 e1000_write_nvm_spi(struct e1000_hw *hw, u16 offset, u16 words, u16 *data)
{
	struct e1000_nvm_info *nvm = &hw->nvm;
	s32 ret_val = -E1000_ERR_NVM;
	u16 widx = 0;

	DEBUGFUNC("e1000_write_nvm_spi");

	/*
	 * A check for invalid values:  offset too large, too many words,
	 * and not enough words.
	 */
	if ((offset >= nvm->word_size) || (words > (nvm->word_size - offset)) ||
	    (words == 0)) {
		DEBUGOUT("nvm parameter(s) out of bounds\n");
		return -E1000_ERR_NVM;
	}

	while (widx < words) {
		u8 write_opcode = NVM_WRITE_OPCODE_SPI;

		ret_val = nvm->ops.acquire(hw);
		if (ret_val)
			return ret_val;

		ret_val = e1000_ready_nvm_eeprom(hw);
		if (ret_val) {
			nvm->ops.release(hw);
			return ret_val;
		}

		e1000_standby_nvm(hw);

		/* Send the WRITE ENABLE command (8 bit opcode) */
		e1000_shift_out_eec_bits(hw, NVM_WREN_OPCODE_SPI,
					 nvm->opcode_bits);

		e1000_standby_nvm(hw);

		/*
		 * Some SPI eeproms use the 8th address bit embedded in the
		 * opcode
		 */
		if ((nvm->address_bits == 8) && (offset >= 128))
			write_opcode |= NVM_A8_OPCODE_SPI;

		/* Send the Write command (8-bit opcode + addr) */
		e1000_shift_out_eec_bits(hw, write_opcode, nvm->opcode_bits);
		e1000_shift_out_eec_bits(hw, (u16)((offset + widx) * 2),
					 nvm->address_bits);

		/* Loop to allow for up to whole page write of eeprom */
		while (widx < words) {
			u16 word_out = data[widx];
			word_out = (word_out >> 8) | (word_out << 8);
			e1000_shift_out_eec_bits(hw, word_out, 16);
			widx++;

			if ((((offset + widx) * 2) % nvm->page_size) == 0) {
				e1000_standby_nvm(hw);
				break;
			}
		}
		msec_delay(10);
		nvm->ops.release(hw);
	}

	return ret_val;
}

/**
 *  e1000_read_pba_string_generic - Read device part number
 *  @hw: pointer to the HW structure
 *  @pba_num: pointer to device part number
 *  @pba_num_size: size of part number buffer
 *
 *  Reads the product board assembly (PBA) number from the EEPROM and stores
 *  the value in pba_num.
 **/
s32 e1000_read_pba_string_generic(struct e1000_hw *hw, u8 *pba_num,
				  u32 pba_num_size)
{
	s32 ret_val;
	u16 nvm_data;
	u16 pba_ptr;
	u16 offset;
	u16 length;

	DEBUGFUNC("e1000_read_pba_string_generic");

	if (pba_num == NULL) {
		DEBUGOUT("PBA string buffer was null\n");
		return -E1000_ERR_INVALID_ARGUMENT;
	}

	ret_val = hw->nvm.ops.read(hw, NVM_PBA_OFFSET_0, 1, &nvm_data);
	if (ret_val) {
		DEBUGOUT("NVM Read Error\n");
		return ret_val;
	}

	ret_val = hw->nvm.ops.read(hw, NVM_PBA_OFFSET_1, 1, &pba_ptr);
	if (ret_val) {
		DEBUGOUT("NVM Read Error\n");
		return ret_val;
	}

	/*
	 * if nvm_data is not ptr guard the PBA must be in legacy format which
	 * means pba_ptr is actually our second data word for the PBA number
	 * and we can decode it into an ascii string
	 */
	if (nvm_data != NVM_PBA_PTR_GUARD) {
		DEBUGOUT("NVM PBA number is not stored as string\n");

		/* make sure callers buffer is big enough to store the PBA */
		if (pba_num_size < E1000_PBANUM_LENGTH) {
			DEBUGOUT("PBA string buffer too small\n");
			return E1000_ERR_NO_SPACE;
		}

		/* extract hex string from data and pba_ptr */
		pba_num[0] = (nvm_data >> 12) & 0xF;
		pba_num[1] = (nvm_data >> 8) & 0xF;
		pba_num[2] = (nvm_data >> 4) & 0xF;
		pba_num[3] = nvm_data & 0xF;
		pba_num[4] = (pba_ptr >> 12) & 0xF;
		pba_num[5] = (pba_ptr >> 8) & 0xF;
		pba_num[6] = '-';
		pba_num[7] = 0;
		pba_num[8] = (pba_ptr >> 4) & 0xF;
		pba_num[9] = pba_ptr & 0xF;

		/* put a null character on the end of our string */
		pba_num[10] = '\0';

		/* switch all the data but the '-' to hex char */
		for (offset = 0; offset < 10; offset++) {
			if (pba_num[offset] < 0xA)
				pba_num[offset] += '0';
			else if (pba_num[offset] < 0x10)
				pba_num[offset] += 'A' - 0xA;
		}

		return E1000_SUCCESS;
	}

	ret_val = hw->nvm.ops.read(hw, pba_ptr, 1, &length);
	if (ret_val) {
		DEBUGOUT("NVM Read Error\n");
		return ret_val;
	}

	if (length == 0xFFFF || length == 0) {
		DEBUGOUT("NVM PBA number section invalid length\n");
		return -E1000_ERR_NVM_PBA_SECTION;
	}
	/* check if pba_num buffer is big enough */
	if (pba_num_size < (((u32)length * 2) - 1)) {
		DEBUGOUT("PBA string buffer too small\n");
		return -E1000_ERR_NO_SPACE;
	}

	/* trim pba length from start of string */
	pba_ptr++;
	length--;

	for (offset = 0; offset < length; offset++) {
		ret_val = hw->nvm.ops.read(hw, pba_ptr + offset, 1, &nvm_data);
		if (ret_val) {
			DEBUGOUT("NVM Read Error\n");
			return ret_val;
		}
		pba_num[offset * 2] = (u8)(nvm_data >> 8);
		pba_num[(offset * 2) + 1] = (u8)(nvm_data & 0xFF);
	}
	pba_num[offset * 2] = '\0';

	return E1000_SUCCESS;
}

/**
 *  e1000_read_pba_length_generic - Read device part number length
 *  @hw: pointer to the HW structure
 *  @pba_num_size: size of part number buffer
 *
 *  Reads the product board assembly (PBA) number length from the EEPROM and
 *  stores the value in pba_num_size.
 **/
s32 e1000_read_pba_length_generic(struct e1000_hw *hw, u32 *pba_num_size)
{
	s32 ret_val;
	u16 nvm_data;
	u16 pba_ptr;
	u16 length;

	DEBUGFUNC("e1000_read_pba_length_generic");

	if (pba_num_size == NULL) {
		DEBUGOUT("PBA buffer size was null\n");
		return -E1000_ERR_INVALID_ARGUMENT;
	}

	ret_val = hw->nvm.ops.read(hw, NVM_PBA_OFFSET_0, 1, &nvm_data);
	if (ret_val) {
		DEBUGOUT("NVM Read Error\n");
		return ret_val;
	}

	ret_val = hw->nvm.ops.read(hw, NVM_PBA_OFFSET_1, 1, &pba_ptr);
	if (ret_val) {
		DEBUGOUT("NVM Read Error\n");
		return ret_val;
	}

	 /* if data is not ptr guard the PBA must be in legacy format */
	if (nvm_data != NVM_PBA_PTR_GUARD) {
		*pba_num_size = E1000_PBANUM_LENGTH;
		return E1000_SUCCESS;
	}

	ret_val = hw->nvm.ops.read(hw, pba_ptr, 1, &length);
	if (ret_val) {
		DEBUGOUT("NVM Read Error\n");
		return ret_val;
	}

	if (length == 0xFFFF || length == 0) {
		DEBUGOUT("NVM PBA number section invalid length\n");
		return -E1000_ERR_NVM_PBA_SECTION;
	}

	/*
	 * Convert from length in u16 values to u8 chars, add 1 for NULL,
	 * and subtract 2 because length field is included in length.
	 */
	*pba_num_size = ((u32)length * 2) - 1;

	return E1000_SUCCESS;
}





/**
 *  e1000_read_mac_addr_generic - Read device MAC address
 *  @hw: pointer to the HW structure
 *
 *  Reads the device MAC address from the EEPROM and stores the value.
 *  Since devices with two ports use the same EEPROM, we increment the
 *  last bit in the MAC address for the second port.
 **/
s32 e1000_read_mac_addr_generic(struct e1000_hw *hw)
{
	u32 rar_high;
	u32 rar_low;
	u16 i;

	rar_high = E1000_READ_REG(hw, E1000_RAH(0));
	rar_low = E1000_READ_REG(hw, E1000_RAL(0));

	for (i = 0; i < E1000_RAL_MAC_ADDR_LEN; i++)
		hw->mac.perm_addr[i] = (u8)(rar_low >> (i*8));

	for (i = 0; i < E1000_RAH_MAC_ADDR_LEN; i++)
		hw->mac.perm_addr[i+4] = (u8)(rar_high >> (i*8));

	for (i = 0; i < ETH_ADDR_LEN; i++)
		hw->mac.addr[i] = hw->mac.perm_addr[i];

	return E1000_SUCCESS;
}

/**
 *  e1000_validate_nvm_checksum_generic - Validate EEPROM checksum
 *  @hw: pointer to the HW structure
 *
 *  Calculates the EEPROM checksum by reading/adding each word of the EEPROM
 *  and then verifies that the sum of the EEPROM is equal to 0xBABA.
 **/
s32 e1000_validate_nvm_checksum_generic(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 checksum = 0;
	u16 i, nvm_data;

	DEBUGFUNC("e1000_validate_nvm_checksum_generic");

	for (i = 0; i < (NVM_CHECKSUM_REG + 1); i++) {
		ret_val = hw->nvm.ops.read(hw, i, 1, &nvm_data);
		if (ret_val) {
			DEBUGOUT("NVM Read Error\n");
			return ret_val;
		}
		checksum += nvm_data;
	}

	if (checksum != (u16) NVM_SUM) {
		DEBUGOUT("NVM Checksum Invalid\n");
		return -E1000_ERR_NVM;
	}

	return E1000_SUCCESS;
}

/**
 *  e1000_update_nvm_checksum_generic - Update EEPROM checksum
 *  @hw: pointer to the HW structure
 *
 *  Updates the EEPROM checksum by reading/adding each word of the EEPROM
 *  up to the checksum.  Then calculates the EEPROM checksum and writes the
 *  value to the EEPROM.
 **/
s32 e1000_update_nvm_checksum_generic(struct e1000_hw *hw)
{
	s32 ret_val;
	u16 checksum = 0;
	u16 i, nvm_data;

	DEBUGFUNC("e1000_update_nvm_checksum");

	for (i = 0; i < NVM_CHECKSUM_REG; i++) {
		ret_val = hw->nvm.ops.read(hw, i, 1, &nvm_data);
		if (ret_val) {
			DEBUGOUT("NVM Read Error while updating checksum.\n");
			return ret_val;
		}
		checksum += nvm_data;
	}
	checksum = (u16) NVM_SUM - checksum;
	ret_val = hw->nvm.ops.write(hw, NVM_CHECKSUM_REG, 1, &checksum);
	if (ret_val)
		DEBUGOUT("NVM Write Error while updating checksum.\n");

	return ret_val;
}

/**
 *  e1000_reload_nvm_generic - Reloads EEPROM
 *  @hw: pointer to the HW structure
 *
 *  Reloads the EEPROM by setting the "Reinitialize from EEPROM" bit in the
 *  extended control register.
 **/
static void e1000_reload_nvm_generic(struct e1000_hw *hw)
{
	u32 ctrl_ext;

	DEBUGFUNC("e1000_reload_nvm_generic");

	usec_delay(10);
	ctrl_ext = E1000_READ_REG(hw, E1000_CTRL_EXT);
	ctrl_ext |= E1000_CTRL_EXT_EE_RST;
	E1000_WRITE_REG(hw, E1000_CTRL_EXT, ctrl_ext);
	E1000_WRITE_FLUSH(hw);
}

/**
 *  e1000_get_fw_version - Get firmware version information
 *  @hw: pointer to the HW structure
 *  @fw_vers: pointer to output version structure
 *
 *  unsupported/not present features return 0 in version structure
 **/
void e1000_get_fw_version(struct e1000_hw *hw, struct e1000_fw_version *fw_vers)
{
	u16 eeprom_verh, eeprom_verl, fw_version;
	u16 comb_verh, comb_verl, comb_offset;

	memset(fw_vers, 0, sizeof(struct e1000_fw_version));

	/* this code only applies to certain mac types */
	switch (hw->mac.type) {
	case e1000_i211:
		e1000_read_invm_version(hw, fw_vers);
		return;
	case e1000_82575:
	case e1000_82576:
	case e1000_82580:
	case e1000_i350:
	case e1000_i210:
		break;
	default:
		return;
	}

	/* basic eeprom version numbers */
	hw->nvm.ops.read(hw, NVM_VERSION, 1, &fw_version);
	fw_vers->eep_major = (fw_version & NVM_MAJOR_MASK) >> NVM_MAJOR_SHIFT;
	fw_vers->eep_minor = (fw_version & NVM_MINOR_MASK);

	/* etrack id */
	hw->nvm.ops.read(hw, NVM_ETRACK_WORD, 1, &eeprom_verl);
	hw->nvm.ops.read(hw, (NVM_ETRACK_WORD + 1), 1, &eeprom_verh);
	fw_vers->etrack_id = (eeprom_verh << NVM_ETRACK_SHIFT) | eeprom_verl;

	switch (hw->mac.type) {
	case e1000_i210:
	case e1000_i350:
		/* find combo image version */
		hw->nvm.ops.read(hw, NVM_COMB_VER_PTR, 1, &comb_offset);
		if ((comb_offset != 0x0) &&
		    (comb_offset != NVM_VER_INVALID)) {

			hw->nvm.ops.read(hw, (NVM_COMB_VER_OFF + comb_offset
					 + 1), 1, &comb_verh);
			hw->nvm.ops.read(hw, (NVM_COMB_VER_OFF + comb_offset),
					 1, &comb_verl);

			/* get Option Rom version if it exists and is valid */
			if ((comb_verh && comb_verl) &&
			    ((comb_verh != NVM_VER_INVALID) &&
			     (comb_verl != NVM_VER_INVALID))) {

				fw_vers->or_valid = true;
				fw_vers->or_major =
					comb_verl >> NVM_COMB_VER_SHFT;
				fw_vers->or_build =
					(comb_verl << NVM_COMB_VER_SHFT)
					| (comb_verh >> NVM_COMB_VER_SHFT);
				fw_vers->or_patch =
					comb_verh & NVM_COMB_VER_MASK;
			}
		}
		break;

	default:
		break;
	}
	return;
}

