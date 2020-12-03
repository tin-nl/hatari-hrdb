#include "disassembler.h"

#include "hopper/buffer.h"
#include "hopper/instruction.h"
#include "hopper/decode.h"

const char* instruction_names[Opcode::COUNT] =
{
    "none",
    "abcd",
    "add",
    "adda",
    "addi",
    "addq",
    "addx",
    "and",
    "andi",
    "asl",
    "asr",
    "bcc",
    "bchg",
    "bclr",
    "bcs",
    "beq",
    "bge",
    "bgt",
    "bhi",
    "ble",
    "bls",
    "blt",
    "bmi",
    "bne",
    "bpl",
    "bra",
    "bset",
    "bsr",
    "btst",
    "bvc",
    "bvs",
    "chk",
    "clr",
    "cmp",
    "cmpi",
    "cmpa",
    "cmpm",
    "dbcc",
    "dbcs",
    "dbeq",
    "dbf",
    "dbge",
    "dbgt",
    "dbhi",
    "dble",
    "dbls",
    "dblt",
    "dbmi",
    "dbne",
    "dbpl",
    "dbra",
    "dbvc",
    "dbvs",
    "divs",
    "divu",
    "eor",
    "eori",
    "exg",
    "ext",
    "illegal",
    "jmp",
    "jsr",
    "lea",
    "link",
    "lsl",
    "lsr",
    "move",
    "movea",
    "movem",
    "movep",
    "moveq",
    "muls",
    "mulu",
    "nbcd",
    "neg",
    "negx",
    "nop",
    "not",
    "or",
    "ori",
    "pea",
    "reset",
    "rol",
    "ror",
    "roxl",
    "roxr",
    "rte",
    "rtr",
    "rts",
    "sbcd",
    "scc",
    "scs",
    "seq",
    "sf",
    "sge",
    "sgt",
    "shi",
    "sle",
    "sls",
    "slt",
    "smi",
    "sne",
    "spl",
    "st",
    "stop",
    "sub",
    "suba",
    "subi",
    "subq",
    "subx",
    "svc",
    "svs",
    "swap",
    "tas",
    "trap",
    "trapv",
    "tst",
    "unlk"
};

int Disassembler::decode_buf(buffer_reader& buf, disassembly& disasm, uint32_t address)
{
    while (buf.get_remain() >= 2)
    {
        line line;
        line.address = buf.get_pos() + address;

        // decode uses a copy of the buffer state
        buffer_reader buf_copy(buf);
        decode(buf_copy, line.inst);

        // Handle failure
        disasm.lines.push_back(line);

        buf.advance(line.inst.byte_count);
    }
    return 0;
}

// ----------------------------------------------------------------------------
//	INSTRUCTION ANALYSIS
// ----------------------------------------------------------------------------
// Check if an instruction jumps to another known address, and return that address
bool calc_relative_address(const operand& op, uint32_t inst_address, uint32_t& target_address)
{
    if (op.type == PC_DISP)
    {
        // Relative
        int16_t disp = op.pc_disp.disp;

        // The base PC is always +2 from the instruction address, since
        // the 68000 has already fetched the header word by then
        target_address = inst_address + 2 + disp;
        return true;
    }
    else if (op.type == PC_DISP_INDEX)
    {
        // Relative
        int8_t disp = op.pc_disp_index.disp;

        // The base PC is always +2 from the instruction address, since
        // the 68000 has already fetched the header word by then
        target_address = inst_address + 2 + disp;
        return true;
    }
    if (op.type == RELATIVE_BRANCH)
    {
        // Relative
        int16_t disp = op.relative_branch.disp;

        // The base PC is always +2 from the instruction address, since
        // the 68000 has already fetched the header word by then
        target_address = inst_address + 2 + disp;
        return true;
    }
    return false;
}

// ----------------------------------------------------------------------------
//	INSTRUCTION DISPLAY FORMATTING
// ----------------------------------------------------------------------------
static const char* g_reg_names[] =
{
    "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7",
    "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7"
};

QString to_hex32(uint32_t val)
{
    QString tmp;
    tmp= QString::asprintf("$%x", val);
    return tmp;
}

// ----------------------------------------------------------------------------
void print(const operand& operand, uint32_t inst_address, QTextStream& ref)
{
    switch (operand.type)
    {
        case OpType::D_DIRECT:
            ref << "d" << operand.d_register.reg;
            return;
        case OpType::A_DIRECT:
            ref << "a" << operand.d_register.reg;
            return;
        case OpType::INDIRECT:
            ref << "(a" << operand.indirect.reg << ")";
            return;
        case OpType::INDIRECT_POSTINC:
        ref << "(a" << operand.indirect_postinc.reg << ")+";
            return;
        case OpType::INDIRECT_PREDEC:
            ref << "-(a" << operand.indirect_predec.reg << ")";
            return;
        case OpType::INDIRECT_DISP:
            ref << operand.indirect_disp.disp << "(a" << operand.indirect_disp.reg << ")";
            return;
        case OpType::INDIRECT_INDEX:
            ref << operand.indirect_index.disp << "(a" << operand.indirect_index.a_reg <<
                   ",d" << operand.indirect_index.d_reg << ")";
            return;
        case OpType::ABSOLUTE_WORD:
            // NO CHECK
            if (operand.absolute_word.wordaddr & 0x8000)
                ref << to_hex32(operand.absolute_word.wordaddr) << ".w";
            else
                ref << to_hex32(operand.absolute_word.wordaddr) << ".w";
            return;
        case OpType::ABSOLUTE_LONG:
        {
            ref << to_hex32(operand.absolute_long.longaddr) << ".l";
        /*
            symbol sym;
            if (find_symbol(symbols, operand.absolute_long.longaddr, sym))
                ref <<"%s", sym.label.c_str());
            else
                ref <<"$%x.l",
                    operand.absolute_long.longaddr);
                    */
            return;
        }
        case OpType::PC_DISP:
        {
        /*
            symbol sym;
            uint32_t target_address;
            calc_relative_address(operand, inst_address, target_address);
            if (find_symbol(symbols, target_address, sym))
                ref <<"%s(pc)", sym.label.c_str());
            else
                ref <<"$%x(pc)", target_address);
                */
            ref << operand.pc_disp.disp << "(pc)";
            return;
        }
        case OpType::PC_DISP_INDEX:
        {
        /*
            symbol sym;
            uint32_t target_address;
            calc_relative_address(operand, inst_address, target_address);

            if (find_symbol(symbols, target_address, sym))
            {
                ref <<"%s(pc,d%d.%s)",
                        sym.label.c_str(),
                        operand.pc_disp_index.d_reg,
                        operand.pc_disp_index.is_long ? "l" : "w");
            }
            else
            {
                ref <<"$%x(pc,d%d.%s)",
                    target_address,
                    operand.pc_disp_index.d_reg,
                    operand.pc_disp_index.is_long ? "l" : "w");

            }
            */
            ref << operand.pc_disp_index.disp << "(pc,d" << operand.pc_disp_index.d_reg << ")";
            return;
        }
        case OpType::MOVEM_REG:
        {
            bool first = true;
            for (int i = 0; i < 16; ++i)
                if (operand.movem_reg.reg_mask & (1 << i))
                {
                    if (!first)
                        ref << "/";
                    ref << g_reg_names[i];
                    first = false;
                }
            return;
        }
        case OpType::RELATIVE_BRANCH:
        {
            uint32_t target_address;
            calc_relative_address(operand, inst_address, target_address);
            ref << to_hex32(target_address);
        /*
            symbol sym;
            if (find_symbol(symbols, target_address, sym))
                ref <<"%s", sym.label.c_str());
            else
                ref <<"$%x", target_address);
           */
            return;
        }

        case OpType::IMMEDIATE:
            ref << "#" << to_hex32(operand.imm.val0);
            return;
        case OpType::SR:
            ref << "sr";
            return;
        case OpType::USP:
            ref << "usp";
            return;
        case OpType::CCR:
            ref << "ccr";
            return;
        default:
            ref << "???";
    }
}

// ----------------------------------------------------------------------------
void Disassembler::print(const instruction& inst, /*const symbols& symbols, */ uint32_t inst_address, QTextStream& ref)
{
    if (inst.opcode == Opcode::NONE)
    {
        ref << "dc.w $%x" << inst.header;
        return;
    }
    ref << instruction_names[inst.opcode];
    switch (inst.suffix)
    {
        case Suffix::BYTE:
            ref << ".b"; break;
        case Suffix::WORD:
            ref << ".w"; break;
        case Suffix::LONG:
            ref << ".l"; break;
        case Suffix::SHORT:
            ref << ".s"; break;
        default:
            break;
    }

    if (inst.op0.type != OpType::INVALID)
    {
        ref << " ";
        ::print(inst.op0, /*symbols,*/ inst_address, ref);
    }

    if (inst.op1.type != OpType::INVALID)
    {
        ref << ",";
        ::print(inst.op1, /*symbols,*/ inst_address, ref);
    }
}
