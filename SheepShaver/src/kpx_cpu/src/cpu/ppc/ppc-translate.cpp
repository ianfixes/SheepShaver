/*
 *  ppc-translate.cpp - PowerPC dynamic translation
 *
 *  Kheperix (C) 2003 Gwenole Beauchesne
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "sysdeps.h"
#include "cpu/ppc/ppc-cpu.hpp"
#include "cpu/ppc/ppc-instructions.hpp"
#include "cpu/ppc/ppc-operands.hpp"

#include <stdio.h>

#define DEBUG 1
#include "debug.h"

#if ENABLE_MON
#include "mon.h"
#include "mon_disass.h"
#endif


/**
 *		Basic block disassemblers
 **/

#define TARGET_M68K		0
#define TARGET_POWERPC	1
#define TARGET_X86		2
#define TARGET_AMD64	3
#if defined(i386) || defined(__i386__)
#define TARGET_NATIVE	TARGET_X86
#endif
#if defined(x86_64) || defined(__x86_64__)
#define TARGET_NATIVE	TARGET_AMD64
#endif
#if defined(powerpc) || defined(__powerpc__)
#define TARGET_NATIVE	TARGET_POWERPC
#endif

static void disasm_block(int target, uint8 *start, uint32 length)
{
#if ENABLE_MON
	char disasm_str[200];
	sprintf(disasm_str, "%s $%x $%x",
			target == TARGET_M68K ? "d68" :
			target == TARGET_X86 ? "d86" :
			target == TARGET_AMD64 ? "d8664" :
			target == TARGET_POWERPC ? "d" : "x",
			start, start + length - 1);

	char *arg[] = {"mon",
#ifdef SHEEPSHAVER
				   "-m",
#endif
				   "-r", disasm_str, NULL};
	mon(sizeof(arg)/sizeof(arg[0]) - 1, arg);
#endif
}

static void disasm_translation(uint32 src_addr, uint32 src_len,
							   uint8* dst_addr, uint32 dst_len)
{
	printf("### Block at %08x translated to %p (%d bytes)\n", src_addr, dst_addr, dst_len);
	printf("IN:\n");
	disasm_block(TARGET_POWERPC, vm_do_get_real_address(src_addr), src_len);
	printf("OUT:\n");
	disasm_block(TARGET_NATIVE, dst_addr, dst_len);
}


/**
 *		DynGen dynamic code translation
 **/

#if PPC_ENABLE_JIT
powerpc_cpu::block_info *
powerpc_cpu::compile_block(uint32 entry_point)
{
#if DEBUG
	bool disasm = false;
#else
	const bool disasm = false;
#endif

#if PPC_PROFILE_COMPILE_TIME
	compile_count++;
	clock_t start_time = clock();
#endif
	block_info *bi = block_cache.new_blockinfo();
	bi->init(entry_point);

  again:
	powerpc_dyngen & dg = codegen;
	bi->entry_point = dg.gen_start();
	const instr_info_t *ii;

	codegen_context_t cg_context(dg);
	cg_context.entry_point = entry_point;

	uint32 dpc = entry_point - 4;
	int pc_offset = 0;
	do {
		uint32 opcode = vm_read_memory_4(dpc += 4);
		ii = decode(opcode);
#if PPC_FLIGHT_RECORDER
		if (is_logging())
			dg.gen_invoke_CPU_im(nv_mem_fun(&powerpc_cpu::record_step).ptr(), opcode);
#endif

		union operands_t {
			struct {
				int size, sign;
				int do_update;
				int do_indexed;
			} mem;
		};
		operands_t op;

		pc_offset += 4;
		switch (ii->mnemo) {
		case PPC_I(LBZ):		// Load Byte and Zero
			op.mem.size = 1;
			op.mem.sign = 0;
			op.mem.do_update = 0;
			op.mem.do_indexed = 0;
			goto do_load;
		case PPC_I(LBZU):		// Load Byte and Zero with Update
			op.mem.size = 1;
			op.mem.sign = 0;
			op.mem.do_update = 1;
			op.mem.do_indexed = 0;
			goto do_load;
		case PPC_I(LBZUX):		// Load Byte and Zero with Update Indexed
			op.mem.size = 1;
			op.mem.sign = 0;
			op.mem.do_update = 1;
			op.mem.do_indexed = 1;
			goto do_load;
		case PPC_I(LBZX):		// Load Byte and Zero Indexed
			op.mem.size = 1;
			op.mem.sign = 0;
			op.mem.do_update = 0;
			op.mem.do_indexed = 1;
			goto do_load;
		case PPC_I(LHA):		// Load Half Word Algebraic
			op.mem.size = 2;
			op.mem.sign = 1;
			op.mem.do_update = 0;
			op.mem.do_indexed = 0;
			goto do_load;
		case PPC_I(LHAU):		// Load Half Word Algebraic with Update
			op.mem.size = 2;
			op.mem.sign = 1;
			op.mem.do_update = 1;
			op.mem.do_indexed = 0;
			goto do_load;
		case PPC_I(LHAUX):		// Load Half Word Algebraic with Update Indexed
			op.mem.size = 2;
			op.mem.sign = 1;
			op.mem.do_update = 1;
			op.mem.do_indexed = 1;
			goto do_load;
		case PPC_I(LHAX):		// Load Half Word Algebraic Indexed
			op.mem.size = 2;
			op.mem.sign = 1;
			op.mem.do_update = 0;
			op.mem.do_indexed = 1;
			goto do_load;
		case PPC_I(LHZ):		// Load Half Word and Zero
			op.mem.size = 2;
			op.mem.sign = 0;
			op.mem.do_update = 0;
			op.mem.do_indexed = 0;
			goto do_load;
		case PPC_I(LHZU):		// Load Half Word and Zero with Update
			op.mem.size = 2;
			op.mem.sign = 0;
			op.mem.do_update = 1;
			op.mem.do_indexed = 0;
			goto do_load;
		case PPC_I(LHZUX):		// Load Half Word and Zero with Update Indexed
			op.mem.size = 2;
			op.mem.sign = 0;
			op.mem.do_update = 1;
			op.mem.do_indexed = 1;
			goto do_load;
		case PPC_I(LHZX):		// Load Half Word and Zero Indexed
			op.mem.size = 2;
			op.mem.sign = 0;
			op.mem.do_update = 0;
			op.mem.do_indexed = 1;
			goto do_load;
		case PPC_I(LWZ):		// Load Word and Zero
			op.mem.size = 4;
			op.mem.sign = 0;
			op.mem.do_update = 0;
			op.mem.do_indexed = 0;
			goto do_load;
		case PPC_I(LWZU):		// Load Word and Zero with Update
			op.mem.size = 4;
			op.mem.sign = 0;
			op.mem.do_update = 1;
			op.mem.do_indexed = 0;
			goto do_load;
		case PPC_I(LWZUX):		// Load Word and Zero with Update Indexed
			op.mem.size = 4;
			op.mem.sign = 0;
			op.mem.do_update = 1;
			op.mem.do_indexed = 1;
			goto do_load;
		case PPC_I(LWZX):		// Load Word and Zero Indexed
			op.mem.size = 4;
			op.mem.sign = 0;
			op.mem.do_update = 0;
			op.mem.do_indexed = 1;
			goto do_load;
		{
		  do_load:
			// Extract RZ operand
			const int rA = rA_field::extract(opcode);
			if (rA == 0 && !op.mem.do_update)
				dg.gen_mov_32_A0_im(0);
			else
				dg.gen_load_A0_GPR(rA);

			// Extract index operand
			if (op.mem.do_indexed)
				dg.gen_load_T1_GPR(rB_field::extract(opcode));

			switch (op.mem.size) {
			case 1:
				if (op.mem.do_indexed)
					dg.gen_load_u8_T0_A0_T1();
				else
					dg.gen_load_u8_T0_A0_im(operand_D::get(this, opcode));
				break;
			case 2:
				if (op.mem.do_indexed) {
					if (op.mem.sign)
						dg.gen_load_s16_T0_A0_T1();
					else
						dg.gen_load_u16_T0_A0_T1();
				}
				else {
					const int32 offset = operand_D::get(this, opcode);
					if (op.mem.sign)
						dg.gen_load_s16_T0_A0_im(offset);
					else
						dg.gen_load_u16_T0_A0_im(offset);
				}
				break;
			case 4:
				if (op.mem.do_indexed) {
					dg.gen_load_u32_T0_A0_T1();
				}
				else {
					const int32 offset = operand_D::get(this, opcode);
					dg.gen_load_u32_T0_A0_im(offset);
				}
				break;
			}

			// Commit result
			dg.gen_store_T0_GPR(rD_field::extract(opcode));

			// Update RA
			if (op.mem.do_update) {
				if (op.mem.do_indexed)
					dg.gen_add_32_A0_T1();
				else
					dg.gen_add_32_A0_im(operand_D::get(this, opcode));
				dg.gen_store_A0_GPR(rA);
			}
			break;
		}
		case PPC_I(STB):		// Store Byte
			op.mem.size = 1;
			op.mem.do_update = 0;
			op.mem.do_indexed = 0;
			goto do_store;
		case PPC_I(STBU):		// Store Byte with Update
			op.mem.size = 1;
			op.mem.do_update = 1;
			op.mem.do_indexed = 0;
			goto do_store;
		case PPC_I(STBUX):		// Store Byte with Update Indexed
			op.mem.size = 1;
			op.mem.do_update = 1;
			op.mem.do_indexed = 1;
			goto do_store;
		case PPC_I(STBX):		// Store Byte Indexed
			op.mem.size = 1;
			op.mem.do_update = 0;
			op.mem.do_indexed = 1;
			goto do_store;
		case PPC_I(STH):		// Store Half Word
			op.mem.size = 2;
			op.mem.do_update = 0;
			op.mem.do_indexed = 0;
			goto do_store;
		case PPC_I(STHU):		// Store Half Word with Update
			op.mem.size = 2;
			op.mem.do_update = 1;
			op.mem.do_indexed = 0;
			goto do_store;
		case PPC_I(STHUX):		// Store Half Word with Update Indexed
			op.mem.size = 2;
			op.mem.do_update = 1;
			op.mem.do_indexed = 1;
			goto do_store;
		case PPC_I(STHX):		// Store Half Word Indexed
			op.mem.size = 2;
			op.mem.do_update = 0;
			op.mem.do_indexed = 1;
			goto do_store;
		case PPC_I(STW):		// Store Word
			op.mem.size = 4;
			op.mem.do_update = 0;
			op.mem.do_indexed = 0;
			goto do_store;
		case PPC_I(STWU):		// Store Word with Update
			op.mem.size = 4;
			op.mem.do_update = 1;
			op.mem.do_indexed = 0;
			goto do_store;
		case PPC_I(STWUX):		// Store Word with Update Indexed
			op.mem.size = 4;
			op.mem.do_update = 1;
			op.mem.do_indexed = 1;
			goto do_store;
		case PPC_I(STWX):		// Store Word Indexed
			op.mem.size = 4;
			op.mem.do_update = 0;
			op.mem.do_indexed = 1;
			goto do_store;
		{
		  do_store:
			// Extract RZ operand
			const int rA = rA_field::extract(opcode);
			if (rA == 0 && !op.mem.do_update)
				dg.gen_mov_32_A0_im(0);
			else
				dg.gen_load_A0_GPR(rA);

			// Extract index operand
			if (op.mem.do_indexed)
				dg.gen_load_T1_GPR(rB_field::extract(opcode));

			// Load register to commit to memory
			dg.gen_load_T0_GPR(rS_field::extract(opcode));

			switch (op.mem.size) {
			case 1:
				if (op.mem.do_indexed)
					dg.gen_store_8_T0_A0_T1();
				else
					dg.gen_store_8_T0_A0_im(operand_D::get(this, opcode));
				break;
			case 2:
				if (op.mem.do_indexed)
					dg.gen_store_16_T0_A0_T1();
				else
					dg.gen_store_16_T0_A0_im(operand_D::get(this, opcode));
				break;
			case 4:
				if (op.mem.do_indexed)
					dg.gen_store_32_T0_A0_T1();
				else
					dg.gen_store_32_T0_A0_im(operand_D::get(this, opcode));
				break;
			}

			// Update RA
			if (op.mem.do_update) {
				if (op.mem.do_indexed)
					dg.gen_add_32_A0_T1();
				else
					dg.gen_add_32_A0_im(operand_D::get(this, opcode));
				dg.gen_store_A0_GPR(rA);
			}
			break;
		}
		case PPC_I(BC):			// Branch Conditional
			dg.gen_mov_32_A0_im(((AA_field::test(opcode) ? 0 : dpc) + operand_BD::get(this, opcode)) & -4);
			goto do_branch;
		case PPC_I(BCCTR):		// Branch Conditional to Count Register
			dg.gen_load_A0_CTR();
			goto do_branch;
		case PPC_I(BCLR):		// Branch Conditional to Link Register
			dg.gen_load_A0_LR();
			goto do_branch;
		{
		  do_branch:
			// FIXME: something is wrong with the conditions!
			if (BO_CONDITIONAL_BRANCH(BO_field::extract(opcode)))
				goto do_generic;

			const uint32 npc = dpc + 4;
			if (LK_field::test(opcode))
				dg.gen_store_im_LR(npc);

			const int bo = BO_field::extract(opcode);
			const int bi = BI_field::extract(opcode);
			dg.gen_bc_A0(bo, bi, npc);
			break;
		}
		case PPC_I(B):			// Branch
		{
			// TODO: follow constant branches
			const uint32 npc = dpc + 4;
			if (LK_field::test(opcode))
				dg.gen_store_im_LR(npc);

			uint32 tpc = AA_field::test(opcode) ? 0 : dpc;
			tpc = (tpc + operand_LI::get(this, opcode)) & -4;
			dg.gen_mov_32_A0_im(tpc);

			// BO field is built so that we always branch to A0
			dg.gen_bc_A0(BO_MAKE(0,0,0,0), 0, 0);
			break;
		}
		case PPC_I(CMP):		// Compare
		{
			dg.gen_load_T0_GPR(rA_field::extract(opcode));
			dg.gen_load_T1_GPR(rB_field::extract(opcode));
			dg.gen_compare_T0_T1(crfD_field::extract(opcode));
			break;
		}
		case PPC_I(CMPI):		// Compare Immediate
		{
			dg.gen_load_T0_GPR(rA_field::extract(opcode));
			dg.gen_compare_T0_im(crfD_field::extract(opcode), operand_SIMM::get(this, opcode));
			break;
		}
		case PPC_I(CMPL):		// Compare Logical
		{
			dg.gen_load_T0_GPR(rA_field::extract(opcode));
			dg.gen_load_T1_GPR(rB_field::extract(opcode));
			dg.gen_compare_logical_T0_T1(crfD_field::extract(opcode));
			break;
		}
		case PPC_I(CMPLI):		// Compare Logical Immediate
		{
			dg.gen_load_T0_GPR(rA_field::extract(opcode));
			dg.gen_compare_logical_T0_im(crfD_field::extract(opcode), operand_UIMM::get(this, opcode));
			break;
		}
		case PPC_I(CRAND):		// Condition Register AND
		case PPC_I(CRANDC):		// Condition Register AND with Complement
		case PPC_I(CREQV):		// Condition Register Equivalent
		case PPC_I(CRNAND):		// Condition Register NAND
		case PPC_I(CRNOR):		// Condition Register NOR
		case PPC_I(CROR):		// Condition Register OR
		case PPC_I(CRORC):		// Condition Register OR with Complement
		case PPC_I(CRXOR):		// Condition Register XOR
		{
			dg.gen_commit_cr();
			dg.gen_load_T0_crb(crbA_field::extract(opcode));
			dg.gen_load_T1_crb(crbB_field::extract(opcode));
			switch (ii->mnemo) {
			case PPC_I(CRAND):	dg.gen_and_32_T0_T1();	break;
			case PPC_I(CRANDC):	dg.gen_andc_32_T0_T1();	break;
			case PPC_I(CREQV):	dg.gen_eqv_32_T0_T1();	break;
			case PPC_I(CRNAND):	dg.gen_nand_32_T0_T1();	break;
			case PPC_I(CRNOR):	dg.gen_nor_32_T0_T1();	break;
			case PPC_I(CROR):	dg.gen_or_32_T0_T1();	break;
			case PPC_I(CRORC):	dg.gen_orc_32_T0_T1();	break;
			case PPC_I(CRXOR):	dg.gen_xor_32_T0_T1();	break;
			default: abort();
			}
			dg.gen_store_T0_crb(crbD_field::extract(opcode));
			break;
		}
		case PPC_I(AND):		// AND
		case PPC_I(ANDC):		// AND with Complement
		case PPC_I(EQV):		// Equivalent
		case PPC_I(NAND):		// NAND
		case PPC_I(NOR):		// NOR
		case PPC_I(ORC):		// ORC
		case PPC_I(XOR):		// XOR
		{
			dg.gen_load_T0_GPR(rS_field::extract(opcode));
			dg.gen_load_T1_GPR(rB_field::extract(opcode));
			switch (ii->mnemo) {
			case PPC_I(AND):	dg.gen_and_32_T0_T1();	break;
			case PPC_I(ANDC):	dg.gen_andc_32_T0_T1();	break;
			case PPC_I(EQV):	dg.gen_eqv_32_T0_T1();	break;
			case PPC_I(NAND):	dg.gen_nand_32_T0_T1();	break;
			case PPC_I(NOR):	dg.gen_nor_32_T0_T1();	break;
			case PPC_I(ORC):	dg.gen_orc_32_T0_T1();	break;
			case PPC_I(XOR):	dg.gen_xor_32_T0_T1();	break;
			default: abort();
			}
			dg.gen_store_T0_GPR(rA_field::extract(opcode));
			if (Rc_field::test(opcode))
				dg.gen_record_cr0_T0();
			break;
		}
		case PPC_I(OR):			// OR
		{
			const int rS = rS_field::extract(opcode);
			const int rB = rB_field::extract(opcode);
			const int rA = rA_field::extract(opcode);
			dg.gen_load_T0_GPR(rS);
			if (rS != rB) {		// Not MR case
				dg.gen_load_T1_GPR(rB);
				dg.gen_or_32_T0_T1();
			}
			dg.gen_store_T0_GPR(rA);
			if (Rc_field::test(opcode))
				dg.gen_record_cr0_T0();
			break;
		}
		case PPC_I(ORI):		// OR Immediate
		{
			const int rA = rA_field::extract(opcode);
			const int rS = rS_field::extract(opcode);
			const uint32 val = operand_UIMM::get(this, opcode);
			if (val == 0) {
				if (rA != rS) { // Skip NOP, handle register move
					dg.gen_load_T0_GPR(rS);
					dg.gen_store_T0_GPR(rA);
				}
			}
			else {
				dg.gen_load_T0_GPR(rS);
				dg.gen_or_32_T0_im(val);
				dg.gen_store_T0_GPR(rA);
			}
			break;
		}
		case PPC_I(XORI):		// XOR Immediate
		{
			dg.gen_load_T0_GPR(rS_field::extract(opcode));
			dg.gen_xor_32_T0_im(operand_UIMM::get(this, opcode));
			dg.gen_store_T0_GPR(rA_field::extract(opcode));
			break;
		}
		case PPC_I(ORIS):		// OR Immediate Shifted
		case PPC_I(XORIS):		// XOR Immediate Shifted
		{
			dg.gen_load_T0_GPR(rS_field::extract(opcode));
			uint32 val = operand_UIMM_shifted::get(this, opcode);
			switch (ii->mnemo) {
			case PPC_I(ORIS):	dg.gen_or_32_T0_im(val);	break;
			case PPC_I(XORIS):	dg.gen_xor_32_T0_im(val);	break;
			default: abort();
			}
			dg.gen_store_T0_GPR(rA_field::extract(opcode));
			break;
		}
		case PPC_I(ANDI):		// AND Immediate
		{
			dg.gen_load_T0_GPR(rS_field::extract(opcode));
			dg.gen_and_32_T0_im(operand_UIMM::get(this, opcode));
			dg.gen_store_T0_GPR(rA_field::extract(opcode));
			dg.gen_record_cr0_T0();
			break;
		}
		case PPC_I(ANDIS):		// AND Immediate Shifted
		{
			dg.gen_load_T0_GPR(rS_field::extract(opcode));
			dg.gen_and_32_T0_im(operand_UIMM_shifted::get(this, opcode));
			dg.gen_store_T0_GPR(rA_field::extract(opcode));
			dg.gen_record_cr0_T0();
			break;
		}
		case PPC_I(EXTSB):		// Extend Sign Byte
		case PPC_I(EXTSH):		// Extend Sign Half Word
		{
			dg.gen_load_T0_GPR(rS_field::extract(opcode));
			switch (ii->mnemo) {
			case PPC_I(EXTSB):	dg.gen_se_8_32_T0();	break;
			case PPC_I(EXTSH):	dg.gen_se_16_32_T0();	break;
			default: abort();
			}
			dg.gen_store_T0_GPR(rA_field::extract(opcode));
			if (Rc_field::test(opcode))
				dg.gen_record_cr0_T0();
			break;
		}
		case PPC_I(NEG):		// Negate
		{
			dg.gen_load_T0_GPR(rA_field::extract(opcode));
			if (OE_field::test(opcode))
				dg.gen_record_nego_T0();
			dg.gen_neg_32_T0();
			if (Rc_field::test(opcode))
				dg.gen_record_cr0_T0();
			dg.gen_store_T0_GPR(rD_field::extract(opcode));
			break;
		}
		case PPC_I(MFCR):		// Move from Condition Register
		{
			dg.gen_commit_cr();
			dg.gen_load_T0_CR();
			dg.gen_store_T0_GPR(rD_field::extract(opcode));
			break;
		}
		case PPC_I(MFSPR):		// Move from Special-Purpose Register
		{
			const int spr = operand_SPR::get(this, opcode);
			switch (spr) {
			case powerpc_registers::SPR_XER:
				dg.gen_load_T0_XER();
				break;
			case powerpc_registers::SPR_LR:
				dg.gen_load_T0_LR();
				break;
			case powerpc_registers::SPR_CTR:
				dg.gen_load_T0_CTR();
				break;
#ifdef SHEEPSHAVER
			case powerpc_registers::SPR_SDR1:
				dg.gen_mov_32_T0_im(0xdead001f);
				break;
			case powerpc_registers::SPR_PVR: {
				extern uint32 PVR;
				dg.gen_mov_32_T0_im(PVR);
				break;
			}
			default:
				dg.gen_mov_32_T0_im(0);
				break;
#else
			default: goto do_illegal;
#endif
			}
			dg.gen_store_T0_GPR(rD_field::extract(opcode));
			break;
		}
		case PPC_I(MTSPR):		// Move to Special-Purpose Register
		{
			dg.gen_load_T0_GPR(rS_field::extract(opcode));
			const int spr = operand_SPR::get(this, opcode);
			switch (spr) {
			case powerpc_registers::SPR_XER:
				dg.gen_store_T0_XER();
				break;
			case powerpc_registers::SPR_LR:
				dg.gen_store_T0_LR();
				break;
			case powerpc_registers::SPR_CTR:
				dg.gen_store_T0_CTR();
				break;
#ifndef SHEEPSHAVER
			default: goto do_illegal;
#endif
			}
			break;
		}
		case PPC_I(ADD):		// Add
		case PPC_I(ADDC):		// Add Carrying
		case PPC_I(ADDE):		// Add Extended
		case PPC_I(SUBF):		// Subtract From
		case PPC_I(SUBFC):		// Subtract from Carrying
		case PPC_I(SUBFE):		// Subtract from Extended
		case PPC_I(MULLW):		// Multiply Low Word
		case PPC_I(DIVW):		// Divide Word
		case PPC_I(DIVWU):		// Divide Word Unsigned
		{
			dg.gen_load_T0_GPR(rA_field::extract(opcode));
			dg.gen_load_T1_GPR(rB_field::extract(opcode));
			if (OE_field::test(opcode)) {
				switch (ii->mnemo) {
				case PPC_I(ADD):	dg.gen_addo_T0_T1();	break;
				case PPC_I(ADDC):	dg.gen_addco_T0_T1();	break;
				case PPC_I(ADDE):	dg.gen_addeo_T0_T1();	break;
				case PPC_I(SUBF):	dg.gen_subfo_T0_T1();	break;
				case PPC_I(SUBFC):	dg.gen_subfco_T0_T1();	break;
				case PPC_I(SUBFE):	dg.gen_subfeo_T0_T1();	break;
				case PPC_I(MULLW):	dg.gen_mullwo_T0_T1();	break;
				case PPC_I(DIVW):	dg.gen_divwo_T0_T1();	break;
				case PPC_I(DIVWU):	dg.gen_divwuo_T0_T1();	break;
				default: abort();
				}
			}
			else {
				switch (ii->mnemo) {
				case PPC_I(ADD):	dg.gen_add_32_T0_T1();	break;
				case PPC_I(ADDC):	dg.gen_addc_T0_T1();	break;
				case PPC_I(ADDE):	dg.gen_adde_T0_T1();	break;
				case PPC_I(SUBF):	dg.gen_subf_T0_T1();	break;
				case PPC_I(SUBFC):	dg.gen_subfc_T0_T1();	break;
				case PPC_I(SUBFE):	dg.gen_subfe_T0_T1();	break;
				case PPC_I(MULLW):	dg.gen_umul_32_T0_T1();	break;
				case PPC_I(DIVW):	dg.gen_divw_T0_T1();	break;
				case PPC_I(DIVWU):	dg.gen_divwu_T0_T1();	break;
				default: abort();
				}
			}
			if (Rc_field::test(opcode))
				dg.gen_record_cr0_T0();
			dg.gen_store_T0_GPR(rD_field::extract(opcode));
			break;
		}
		case PPC_I(ADDIC):		// Add Immediate Carrying
		case PPC_I(ADDIC_):		// Add Immediate Carrying and Record
		case PPC_I(SUBFIC):		// Subtract from Immediate Carrying
		{
			dg.gen_load_T0_GPR(rA_field::extract(opcode));
			const uint32 val = operand_SIMM::get(this, opcode);
			switch (ii->mnemo) {
			case PPC_I(ADDIC):
				dg.gen_addc_T0_im(val);
				break;
			case PPC_I(ADDIC_):
				dg.gen_addc_T0_im(val);
				dg.gen_record_cr0_T0();
				break;
			case PPC_I(SUBFIC):
				dg.gen_subfc_T0_im(val);
				break;
			  defautl:
				abort();
			}
			dg.gen_store_T0_GPR(rD_field::extract(opcode));
			break;
		}
		case PPC_I(ADDME):		// Add to Minus One Extended
		case PPC_I(ADDZE):		// Add to Zero Extended
		case PPC_I(SUBFME):		// Subtract from Minus One Extended
		case PPC_I(SUBFZE):		// Subtract from Zero Extended
		{
			dg.gen_load_T0_GPR(rA_field::extract(opcode));
			if (OE_field::test(opcode)) {
				switch (ii->mnemo) {
				case PPC_I(ADDME):	dg.gen_addmeo_T0();		break;
				case PPC_I(ADDZE):	dg.gen_addzeo_T0();		break;
				case PPC_I(SUBFME):	dg.gen_subfmeo_T0();	break;
				case PPC_I(SUBFZE):	dg.gen_subfzeo_T0();	break;
				default: abort();
				}
			}
			else {
				switch (ii->mnemo) {
				case PPC_I(ADDME):	dg.gen_addme_T0();		break;
				case PPC_I(ADDZE):	dg.gen_addze_T0();		break;
				case PPC_I(SUBFME):	dg.gen_subfme_T0();		break;
				case PPC_I(SUBFZE):	dg.gen_subfze_T0();		break;
				default: abort();
				}
			}
			if (Rc_field::test(opcode))
				dg.gen_record_cr0_T0();
			dg.gen_store_T0_GPR(rD_field::extract(opcode));
			break;
		}
		case PPC_I(ADDI):		// Add Immediate
		{
			const int rA = rA_field::extract(opcode);
			const int rD = rD_field::extract(opcode);
			if (rA == 0)		// li rD,value
				dg.gen_mov_32_T0_im(operand_SIMM::get(this, opcode));
			else {
				dg.gen_load_T0_GPR(rA);
				dg.gen_add_32_T0_im(operand_SIMM::get(this, opcode));
			}
			dg.gen_store_T0_GPR(rD);
			break;
		}
		case PPC_I(ADDIS):		// Add Immediate Shifted
		{
			const int rA = rA_field::extract(opcode);
			const int rD = rD_field::extract(opcode);
			if (rA == 0)		// lis rD,value
				dg.gen_mov_32_T0_im(operand_SIMM_shifted::get(this, opcode));
			else {
				dg.gen_load_T0_GPR(rA);
				dg.gen_add_32_T0_im(operand_SIMM_shifted::get(this, opcode));
			}
			dg.gen_store_T0_GPR(rD);
			break;
		}
		case PPC_I(RLWIMI):		// Rotate Left Word Immediate then Mask Insert
		{
			const int rA = rA_field::extract(opcode);
			const int rS = rS_field::extract(opcode);
			const int SH = SH_field::extract(opcode);
			const int MB = MB_field::extract(opcode);
			const int ME = ME_field::extract(opcode);
			dg.gen_load_T0_GPR(rA);
			dg.gen_load_T1_GPR(rS);
			const uint32 m = mask_operand::compute(MB, ME);
			dg.gen_rlwimi_T0_T1(SH, m);
			dg.gen_store_T0_GPR(rA);
			if (Rc_field::test(opcode))
				dg.gen_record_cr0_T0();
			break;
		}
		case PPC_I(RLWINM):		// Rotate Left Word Immediate then AND with Mask
		{
			const int rS = rS_field::extract(opcode);
			const int rA = rA_field::extract(opcode);
			const int SH = SH_field::extract(opcode);
			const int MB = MB_field::extract(opcode);
			const int ME = ME_field::extract(opcode);
			dg.gen_load_T0_GPR(rS);
			if (MB == 0 && ME == 31) {
				// rotlwi rA,rS,SH
				if (SH > 0)
					dg.gen_rol_32_T0_im(SH);
			}
			else if (MB == 0 && (ME == (31 - SH))) {
				// slwi rA,rS,SH
				dg.gen_lsl_32_T0_im(SH);
			}
			else {
				const uint32 m = mask_operand::compute(MB, ME);
				if (SH == 0) {
					// andi rA,rS,MASK(MB,ME)
					dg.gen_and_32_T0_im(m);
				}
				else {
					// rlwinm rA,rS,SH,MB,ME
					dg.gen_rlwinm_T0_T1(SH, m);
				}
			}
			dg.gen_store_T0_GPR(rA);
			if (Rc_field::test(opcode))
				dg.gen_record_cr0_T0();
			break;
		}
		case PPC_I(RLWNM):		// Rotate Left Word then AND with Mask
		{
			const int rS = rS_field::extract(opcode);
			const int rB = rB_field::extract(opcode);
			const int rA = rA_field::extract(opcode);
			const uint32 m = operand_MASK::get(this, opcode);
			dg.gen_load_T0_GPR(rS);
			dg.gen_load_T1_GPR(rB);
			dg.gen_rlwnm_T0_T1(m);
			dg.gen_store_T0_GPR(rA);
			if (Rc_field::test(opcode))
				dg.gen_record_cr0_T0();
			break;
		}
		case PPC_I(CNTLZW):		// Count Leading Zeros Word
		{
			dg.gen_load_T0_GPR(rS_field::extract(opcode));
			dg.gen_cntlzw_32_T0();
			dg.gen_store_T0_GPR(rA_field::extract(opcode));
			if (Rc_field::test(opcode))
				dg.gen_record_cr0_T0();
			break;
		}
		case PPC_I(SLW):		// Shift Left Word
		{
			dg.gen_load_T0_GPR(rS_field::extract(opcode));
			dg.gen_load_T1_GPR(rB_field::extract(opcode));
			dg.gen_slw_T0_T1();
			dg.gen_store_T0_GPR(rA_field::extract(opcode));
			if (Rc_field::test(opcode))
				dg.gen_record_cr0_T0();
			break;
		}
		case PPC_I(SRW):		// Shift Right Word
		{
			dg.gen_load_T0_GPR(rS_field::extract(opcode));
			dg.gen_load_T1_GPR(rB_field::extract(opcode));
			dg.gen_srw_T0_T1();
			dg.gen_store_T0_GPR(rA_field::extract(opcode));
			if (Rc_field::test(opcode))
				dg.gen_record_cr0_T0();
			break;
		}
		case PPC_I(SRAW):		// Shift Right Algebraic Word
		{
			dg.gen_load_T0_GPR(rS_field::extract(opcode));
			dg.gen_load_T1_GPR(rB_field::extract(opcode));
			dg.gen_sraw_T0_T1();
			dg.gen_store_T0_GPR(rA_field::extract(opcode));
			if (Rc_field::test(opcode))
				dg.gen_record_cr0_T0();
			break;
		}
		case PPC_I(SRAWI):		// Shift Right Algebraic Word Immediate
		{
			dg.gen_load_T0_GPR(rS_field::extract(opcode));
			dg.gen_sraw_T0_im(SH_field::extract(opcode));
			dg.gen_store_T0_GPR(rA_field::extract(opcode));
			if (Rc_field::test(opcode))
				dg.gen_record_cr0_T0();
			break;
		}
		case PPC_I(MULHW):		// Multiply High Word
		case PPC_I(MULHWU):		// Multiply High Word Unsigned
		{
			dg.gen_load_T0_GPR(rA_field::extract(opcode));
			dg.gen_load_T1_GPR(rB_field::extract(opcode));
			if (ii->mnemo == PPC_I(MULHW))
				dg.gen_mulhw_T0_T1();
			else
				dg.gen_mulhwu_T0_T1();
			dg.gen_store_T0_GPR(rD_field::extract(opcode));
			if (Rc_field::test(opcode))
				dg.gen_record_cr0_T0();
			break;
		}
		case PPC_I(MULLI):		// Multiply Low Immediate
		{
			dg.gen_load_T0_GPR(rA_field::extract(opcode));
			dg.gen_mulli_T0_im(operand_SIMM::get(this, opcode));
			dg.gen_store_T0_GPR(rD_field::extract(opcode));
			break;
		}
		default:				// Direct call to instruction handler
		{
			typedef void (*func_t)(dyngen_cpu_base, uint32);
			func_t func;
		  do_generic:
//			printf("UNHANDLED: %s\n", ii->name);
			func = (func_t)ii->execute.ptr();
			goto do_invoke;
		  do_illegal:
			func = (func_t)nv_mem_fun(&powerpc_cpu::execute_illegal).ptr();
			goto do_invoke;	
		  do_invoke:
			cg_context.pc = dpc;
			cg_context.opcode = opcode;
			cg_context.instr_info = ii;
			if (!compile1(cg_context)) {
				pc_offset -= 4;
				if (pc_offset) {
					dg.gen_inc_PC(pc_offset);
					pc_offset = 0;
				}
				dg.gen_commit_cr();
				dg.gen_invoke_CPU_im(func, opcode);
			}
		}
		}
		if (dg.full_translation_cache()) {
			// Invalidate cache and start again
			invalidate_cache();
			goto again;
		}
	} while ((ii->cflow & CFLOW_END_BLOCK) == 0);
	dg.gen_commit_cr();
	dg.gen_exec_return();
	dg.gen_end();
	bi->end_pc = dpc;
	bi->size = dg.code_ptr() - bi->entry_point;
#if defined(__powerpc__) && 0
	double ratio = double(bi->size) / double(dpc - entry_point + 4);
	if (ratio >= 10)
		disasm = true;
#endif
	if (disasm)
		disasm_translation(entry_point, dpc - entry_point + 4, bi->entry_point, bi->size);
	block_cache.add_to_cl_list(bi);
	block_cache.add_to_active_list(bi);
#if PPC_PROFILE_COMPILE_TIME
	compile_time += (clock() - start_time);
#endif
	return bi;
}
#endif
