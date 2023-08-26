#ifndef AARCH64_INSTRUCTION_DEFINITIONS_HPP
#define AARCH64_INSTRUCTION_DEFINITIONS_HPP

#include "../../InstructionDefinitions.hpp"
#include "../../MachineInstruction.hpp"
#include "../../TargetInstruction.hpp"
#include <map>

namespace AArch64 {

enum Opcodes : unsigned {
  ADD_rrr,
  ADD_rri,
  AND_rrr,
  AND_rri,
  ORR_rrr,
  ORR_rri,
  EOR_rrr,
  EOR_rri,
  LSL_rrr,
  LSL_rri,
  LSR_rrr,
  LSR_rri,
  SUB_rrr,
  SUB_rri,
  SUBS,
  MUL_rri,
  MUL_rrr,
  SDIV_rri,
  SDIV_rrr,
  UDIV_rrr,
  CMP_ri,
  CMP_rr,
  CSET_eq,
  CSET_ne,
  CSET_lt,
  CSET_le,
  CSET_gt,
  CSET_ge,
  SXTB,
  SXTH,
  SXTW,
  UXTB,
  UXTH,
  UXTW,
  MOV_rc,
  MOV_rr,
  MOVK_ri,
  MVN_rr,

  FADD_rrr,
  FSUB_rrr,
  FMUL_rrr,
  FDIV_rrr,
  FMOV_rr,
  FMOV_ri,
  FCMP_rr,
  FCMP_ri,
  SCVTF_rr,
  FCVTZS_rr,

  ADRP,
  LDR,
  LDRB,
  LDRH,
  STR,
  STRB,
  STRH,
  BEQ,
  BNE,
  BGE,
  BGT,
  BLE,
  BLT,
  B,
  BL,
  RET,
};

enum OperandTypes : unsigned {
  GPR,
  GPR32,
  GPR64,
  FPR,
  FPR32,
  FPR64,
  UIMM4,
  SIMM12,
  UIMM12,
  UIMM16,
  SIMM13_LSB0,
  SIMM21_LSB0,
};

class AArch64InstructionDefinitions : public InstructionDefinitions {
  using IRToTargetInstrMap = std::map<unsigned, TargetInstruction>;

public:
  AArch64InstructionDefinitions();
  ~AArch64InstructionDefinitions() override {}
  TargetInstruction *GetTargetInstr(unsigned Opcode) override;
  std::string GetInstrString(unsigned index) override {
    assert(index < InstrEnumStrings.size() && "Out of bound access");
    return InstrEnumStrings[index];
  }

private:
  static IRToTargetInstrMap Instructions;
  std::vector<std::string> InstrEnumStrings;
};

} // namespace AArch64

#endif
