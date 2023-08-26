
#include "TargetMachine.hpp"
#include <cassert>

bool TargetMachine::SelectInstruction(MachineInstruction *MI) {
  auto Opcode = MI->GetOpcode();

  switch (Opcode) {
  case MachineInstruction::AND:
    return SelectAND(MI);
  case MachineInstruction::OR:
    return SelectOR(MI);
  case MachineInstruction::XOR:
    return SelectXOR(MI);
  case MachineInstruction::LSL:
    return SelectLSL(MI);
  case MachineInstruction::LSR:
    return SelectLSR(MI);
  case MachineInstruction::ADD:
    return SelectADD(MI);
  case MachineInstruction::ADDS:
    return SelectADDS(MI);
  case MachineInstruction::ADDC:
    return SelectADDC(MI);
  case MachineInstruction::SUB:
    return SelectSUB(MI);
  case MachineInstruction::MUL:
    return SelectMUL(MI);
  case MachineInstruction::MULHU:
    return SelectMULHU(MI);
  case MachineInstruction::DIV:
    return SelectDIV(MI);
  case MachineInstruction::DIVU:
    return SelectDIVU(MI);
  case MachineInstruction::CMP:
    return SelectCMP(MI);
  case MachineInstruction::MOD:
    return SelectMOD(MI);
  case MachineInstruction::MODU:
    return SelectMODU(MI);
  case MachineInstruction::CMPF:
    return SelectCMPF(MI);
  case MachineInstruction::ADDF:
    return SelectADDF(MI);
  case MachineInstruction::SUBF:
    return SelectSUBF(MI);
  case MachineInstruction::MULF:
    return SelectMULF(MI);
  case MachineInstruction::DIVF:
    return SelectDIVF(MI);
  case MachineInstruction::ITOF:
    return SelectITOF(MI);
  case MachineInstruction::FTOI:
    return SelectFTOI(MI);
  case MachineInstruction::SEXT:
    return SelectSEXT(MI);
  case MachineInstruction::ZEXT:
    return SelectZEXT(MI);
  case MachineInstruction::TRUNC:
    return SelectTRUNC(MI);
  case MachineInstruction::ZEXT_LOAD:
    return SelectZEXT_LOAD(MI);
  case MachineInstruction::LOAD_IMM:
    return SelectLOAD_IMM(MI);
  case MachineInstruction::MOV:
    return SelectMOV(MI);
  case MachineInstruction::MOVF:
    return SelectMOVF(MI);
  case MachineInstruction::LOAD:
    return SelectLOAD(MI);
  case MachineInstruction::STORE:
    return SelectSTORE(MI);
  case MachineInstruction::STACK_ADDRESS:
    return SelectSTACK_ADDRESS(MI);
  case MachineInstruction::GLOBAL_ADDRESS:
    return SelectGLOBAL_ADDRESS(MI);
  case MachineInstruction::BRANCH:
    return SelectBRANCH(MI);
  case MachineInstruction::JUMP:
    return SelectJUMP(MI);
  case MachineInstruction::CALL:
    return SelectCALL(MI);
  case MachineInstruction::RET:
    return SelectRET(MI);
  default:
    assert(!"Unimplemented");
  }

  return false;
}