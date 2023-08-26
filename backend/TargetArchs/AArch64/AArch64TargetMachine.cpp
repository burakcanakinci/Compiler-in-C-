#include "AArch64TargetMachine.hpp"
#include "../../MachineBasicBlock.hpp"
#include "../../MachineFunction.hpp"
#include "../../Support.hpp"
#include "AArch64InstructionDefinitions.hpp"
#include <cassert>

using namespace AArch64;

// TODO: This should be done in the legalizer
void ExtendRegSize(MachineOperand *MO, uint8_t BitWidth = 32) {
  if (MO->GetSize() < 32)
    MO->GetTypeRef().SetBitWidth(BitWidth);
}

/// Materialize the given constant before the MI instruction
/// TODO: AArch64 have strange operands, MOV_rc can encode a 16 bit immediate,
/// but actually it can do more, with added shifts etc. For example it can
/// actually encode 1ull << 50, but not sure yet how this work exactly.
/// Figure it out and implement these exceptional cases.
/// NOTE: It seems like mov is an alias for movz as well! Seems far from being
/// trivial.
MachineInstruction *AArch64TargetMachine::MaterializeConstant(
    MachineInstruction *MI, const uint64_t Constant, MachineOperand &VReg,
    const bool UseVRegAndMI) {
  auto MBB = MI->GetParent();

  // If UseVRegAndMI is false, then a VReg is an out operand, so it must be
  // allocated and set. If it is true, then VReg is already set, so use it
  // without any change of it.
  if (!UseVRegAndMI) {
    auto Reg = MBB->GetParent()->GetNextAvailableVReg();
    VReg = MachineOperand::CreateVirtualRegister(Reg);
    // define its size by the size of the destination register of the MI for now
    // and assume an integer constant
    VReg.SetRegClass(
        RegInfo->GetRegisterClass(MI->GetOperand(0)->GetSize(), false));
  }

  std::vector<MachineInstruction> MIs;

  // If UseVRegAndMI is false, then MI will not be changed. If true then it
  // must be selected to the first instruction in the materialization sequence.
  if (!UseVRegAndMI) {
    MachineInstruction MOV;
    MOV.SetOpcode(MOV_rc);
    MOV.AddOperand(VReg);
    if (!IsInt<16>(Constant))
      MOV.AddImmediate(Constant & 0xffffu); // keep lower 16 bit
    else
      MOV.AddImmediate(Constant);
    MIs.push_back(MOV);
  } else {
    MI->SetOpcode(MOV_rc);
    MI->RemoveOperand(1);
    if (!IsInt<16>(Constant))
      MI->AddImmediate(Constant & 0xffffu); // keep lower 16 bit
    else
      MI->AddImmediate(Constant);
  }

  if (!IsInt<16>(Constant)) {
    MachineInstruction MOVK;
    MOVK.SetOpcode(MOVK_ri);
    MOVK.AddOperand(VReg);
    MOVK.AddImmediate((uint32_t)Constant >> 16u); // upper 16 bit
    MOVK.AddImmediate(16);                        // left shift amount
    MIs.push_back(MOVK);

    if (!IsInt<32>(Constant)) {
      MachineInstruction MOVK;
      MOVK.SetOpcode(MOVK_ri);
      MOVK.AddOperand(VReg);
      MOVK.AddImmediate((Constant >> 32u) & 0xffffu);
      MOVK.AddImmediate(32);
      MIs.push_back(MOVK);

      MachineInstruction MOVK2;
      MOVK2.SetOpcode(MOVK_ri);
      MOVK2.AddOperand(VReg);
      MOVK2.AddImmediate(Constant >> 48u);
      MOVK2.AddImmediate(48);
      MIs.push_back(MOVK2);
    }
  }

  if (UseVRegAndMI)
    return &*MBB->InsertAfter(std::move(MIs), MI);
  return &*MBB->InsertBefore(std::move(MIs), MI);
}

/// For the given MI the function select its rrr or rri variant based on
/// the MI form. If the immediate does not fit into the instruction @ImmSize
/// width long immediate part, then it will be materialized into a register
bool AArch64TargetMachine::SelectThreeAddressInstruction(
    MachineInstruction *MI, const Opcodes rrr, const Opcodes rri,
    const unsigned ImmSize) {
  if (auto ImmMO = MI->GetOperand(2); ImmMO->IsImmediate()) {
    if (IsInt(ImmMO->GetImmediate(), ImmSize)) {
      MI->SetOpcode(rri);
      return true;
    }

    MachineOperand VReg;
    MI = MaterializeConstant(MI, ImmMO->GetImmediate(), VReg);
    MI->SetOpcode(rrr);
    MI->RemoveOperand(2);
    MI->AddOperand(VReg);

    return true;
  } else if (MI->GetOperand(2)->IsRegister() ||
             MI->GetOperand(2)->IsVirtualReg()) {
    MI->SetOpcode(rrr);
    return true;
  }
  return false;
}

bool AArch64TargetMachine::SelectAND(MachineInstruction *MI) {
  assert(MI->GetOperandsNumber() == 3 && "AND must have 3 operands");

  ExtendRegSize(MI->GetOperand(0));
  ExtendRegSize(MI->GetOperand(1));

  if (!SelectThreeAddressInstruction(MI, AND_rrr, AND_rri))
    assert(!"Cannot select AND");

  return true;
}

bool AArch64TargetMachine::SelectOR(MachineInstruction *MI) {
  assert(MI->GetOperandsNumber() == 3 && "OR must have 3 operands");

  ExtendRegSize(MI->GetOperand(0));
  ExtendRegSize(MI->GetOperand(1));

  if (!SelectThreeAddressInstruction(MI, ORR_rrr, ORR_rri))
    assert(!"Cannot select OR");

  return true;
}

bool AArch64TargetMachine::SelectXOR(MachineInstruction *MI) {
  assert(MI->GetOperandsNumber() == 3 && "XOR must have 3 operands");

  ExtendRegSize(MI->GetOperand(0));
  ExtendRegSize(MI->GetOperand(1));

  // in case of bitwise not
  if (MI->GetOperand(2)->IsImmediate() &&
      MI->GetOperand(2)->GetImmediate() == -1) {
    MI->RemoveOperand(2);
    MI->SetOpcode(MVN_rr);
    return true;
  }

  if (!SelectThreeAddressInstruction(MI, EOR_rrr, EOR_rri))
    assert(!"Cannot select XOR");

  return true;
}

bool AArch64TargetMachine::SelectLSL(MachineInstruction *MI) {
  assert(MI->GetOperandsNumber() == 3 && "LSL must have 3 operands");

  ExtendRegSize(MI->GetOperand(0));
  ExtendRegSize(MI->GetOperand(1));

  if (!SelectThreeAddressInstruction(MI, LSL_rrr, LSL_rri))
    assert(!"Cannot select LSL");

  return true;
}

bool AArch64TargetMachine::SelectLSR(MachineInstruction *MI) {
  assert(MI->GetOperandsNumber() == 3 && "LSR must have 3 operands");

  ExtendRegSize(MI->GetOperand(0));
  ExtendRegSize(MI->GetOperand(1));

  if (!SelectThreeAddressInstruction(MI, LSR_rrr, LSR_rri))
    assert(!"Cannot select LSR");

  return true;
}

bool AArch64TargetMachine::SelectADD(MachineInstruction *MI) {
  assert(MI->GetOperandsNumber() == 3 && "ADD must have 3 operands");

  ExtendRegSize(MI->GetOperand(0));
  ExtendRegSize(MI->GetOperand(1));

  if (auto Symbol = MI->GetOperand(2); Symbol->IsGlobalSymbol()) {
    MI->SetOpcode(ADD_rri);
    return true;
  }
  // If last operand is an immediate then select "addi"
  else if (auto ImmMO = MI->GetOperand(2); ImmMO->IsImmediate()) {
    // FIXME: Since currently ADD used for adjusting the stack in the prolog,
    // therefore its possible that the immediate is negative. In that case for
    // now we just convert the ADD into a SUB and call select on that.
    if ((int64_t)ImmMO->GetImmediate() < 0) {
      MI->SetOpcode(SUB_rri);
      MI->GetOperand(2)->SetValue(((int64_t)ImmMO->GetImmediate()) * -1);
      return SelectSUB(MI);
    }
    assert(IsUInt<12>((int64_t)ImmMO->GetImmediate()) &&
           "Immediate must be 12 bit wide");

    // TODO: check if the register operands are valid, like i32 and not f32
    // NOTE: maybe we should not really check here, although then how we know
    // that it is a floating point addition or not?
    MI->SetOpcode(ADD_rri);
    return true;
  }
  // Try to select "add"
  else {
    MI->SetOpcode(ADD_rrr);
    return true;
  }

  return false;
}

bool AArch64TargetMachine::SelectSUB(MachineInstruction *MI) {
  assert(MI->GetOperandsNumber() == 3 && "SUB must have 3 operands");

  ExtendRegSize(MI->GetOperand(0));
  ExtendRegSize(MI->GetOperand(1));

  // If last operand is an immediate then select "SUB_rri"
  if (auto ImmMO = MI->GetOperand(2); ImmMO->IsImmediate()) {
    assert(IsUInt<12>((int64_t)ImmMO->GetImmediate()) &&
           "Immediate must be 12 bit wide");

    // TODO: see ADD comment
    MI->SetOpcode(SUB_rri);
    return true;
  }
  // else try to select "SUB_rrr"
  else {
    MI->SetOpcode(SUB_rrr);
    return true;
  }

  return false;
}

bool AArch64TargetMachine::SelectMUL(MachineInstruction *MI) {
  assert(MI->GetOperandsNumber() == 3 && "MUL must have 3 operands");

  ExtendRegSize(MI->GetOperand(0));
  ExtendRegSize(MI->GetOperand(1));

  // If last operand is an immediate then select "MUL_rri"
  if (auto ImmMO = MI->GetOperand(2); ImmMO->IsImmediate()) {
    assert(IsUInt<12>((int64_t)ImmMO->GetImmediate()) &&
           "Immediate must be 12 bit wide");

    // TODO: see ADD comment
    MI->SetOpcode(MUL_rri);
    return true;
  }
  // else try to select "MUL_rrr"
  else {
    MI->SetOpcode(MUL_rrr);
    return true;
  }

  return false;
}

bool AArch64TargetMachine::SelectDIV(MachineInstruction *MI) {
  assert(MI->GetOperandsNumber() == 3 && "DIV must have 3 operands");

  ExtendRegSize(MI->GetOperand(0));
  ExtendRegSize(MI->GetOperand(1));

  // If last operand is an immediate then select "addi"
  if (auto ImmMO = MI->GetOperand(2); ImmMO->IsImmediate()) {
    assert(IsUInt<12>((int64_t)ImmMO->GetImmediate()) &&
           "Immediate must be 12 bit wide");

    // TODO: see ADD comment
    MI->SetOpcode(SDIV_rri);
    return true;
  }
  // else try to select "SDIV_rrr"
  else {
    MI->SetOpcode(SDIV_rrr);
    return true;
  }

  return false;
}

bool AArch64TargetMachine::SelectDIVU(MachineInstruction *MI) {
  assert(MI->GetOperandsNumber() == 3 && "DIVU must have 3 operands");

  ExtendRegSize(MI->GetOperand(0));
  ExtendRegSize(MI->GetOperand(1));

  // If last operand is an immediate then select "addi"
  if (auto ImmMO = MI->GetOperand(2); ImmMO->IsImmediate()) {
    assert(!"Immediate not supported");
  } else {
    MI->SetOpcode(UDIV_rrr);
    return true;
  }

  return false;
}

bool AArch64TargetMachine::SelectMOD(MachineInstruction *MI) {
  assert(!"MOD not supported");
  return false;
}

bool AArch64TargetMachine::SelectMODU(MachineInstruction *MI) {
  assert(!"MODU not supported");
  return false;
}

bool AArch64TargetMachine::SelectCMP(MachineInstruction *MI) {
  assert(MI->GetOperandsNumber() == 3 && "CMP must have 3 operands");

  ExtendRegSize(MI->GetOperand(0));
  ExtendRegSize(MI->GetOperand(1));

  if (auto ImmMO = MI->GetOperand(2); ImmMO->IsImmediate()) {
    if (IsInt<12>(ImmMO->GetImmediate()))
      MI->SetOpcode(CMP_ri);
    else {
      MachineOperand Reg;
      MI = MaterializeConstant(MI, ImmMO->GetImmediate(), Reg);
      MI->SetOpcode(CMP_rr);
      MI->RemoveOperand(2);
      MI->AddOperand(Reg);
    }
    // remove the destination hence the implicit condition register is
    // overwritten
    MI->RemoveOperand(0);
    return true;
  } else {
    MI->SetOpcode(CMP_rr);
    // remove the destination hence the implicit condition register is
    // overwritten
    MI->RemoveOperand(0);
    return true;
  }

  return false;
}

bool SelectThreeAddressFPInstuction(MachineInstruction *MI, Opcodes rrr) {
  if (auto ImmMO = MI->GetOperand(2); ImmMO->IsImmediate()) {
    return false;
  } else {
    MI->SetOpcode(rrr);
    return true;
  }
}

bool AArch64TargetMachine::SelectCMPF(MachineInstruction *MI) {
  assert(MI->GetOperandsNumber() == 3 && "CMP must have 3 operands");

  ExtendRegSize(MI->GetOperand(0));
  ExtendRegSize(MI->GetOperand(1));

  if (auto ImmMO = MI->GetOperand(2); ImmMO->IsImmediate()) {
    MI->SetOpcode(FCMP_ri);
    MI->RemoveOperand(0);
    return true;
  } else {
    MI->SetOpcode(FCMP_rr);
    MI->RemoveOperand(0);
    return true;
  }

  return false;
}

bool AArch64TargetMachine::SelectADDF(MachineInstruction *MI) {
  assert(MI->GetOperandsNumber() == 3 && "ADDF must have 3 operands");

  ExtendRegSize(MI->GetOperand(0));
  ExtendRegSize(MI->GetOperand(1));

  if (!SelectThreeAddressFPInstuction(MI, FADD_rrr))
    assert(!"Immedaite operand is not allowed for FADD");

  return true;
}

bool AArch64TargetMachine::SelectSUBF(MachineInstruction *MI) {
  assert(MI->GetOperandsNumber() == 3 && "SUBF must have 3 operands");

  ExtendRegSize(MI->GetOperand(0));
  ExtendRegSize(MI->GetOperand(1));

  if (!SelectThreeAddressFPInstuction(MI, FSUB_rrr))
    assert(!"Immedaite operand is not allowed for FSUB");

  return true;
}

bool AArch64TargetMachine::SelectMULF(MachineInstruction *MI) {
  assert(MI->GetOperandsNumber() == 3 && "MULF must have 3 operands");

  ExtendRegSize(MI->GetOperand(0));
  ExtendRegSize(MI->GetOperand(1));

  if (!SelectThreeAddressFPInstuction(MI, FMUL_rrr))
    assert(!"Immedaite operand is not allowed for FMUL");

  return true;
}

bool AArch64TargetMachine::SelectDIVF(MachineInstruction *MI) {
  assert(MI->GetOperandsNumber() == 3 && "DIVF must have 3 operands");

  ExtendRegSize(MI->GetOperand(0));
  ExtendRegSize(MI->GetOperand(1));

  if (!SelectThreeAddressFPInstuction(MI, FDIV_rrr))
    assert(!"Immedaite operand is not allowed for FDIV");

  return true;
}

bool AArch64TargetMachine::SelectITOF(MachineInstruction *MI) {
  assert(MI->GetOperandsNumber() == 2 && "ITOF must have 2 operands");

  ExtendRegSize(MI->GetOperand(0));

  MI->SetOpcode(SCVTF_rr);
  return true;
}

bool AArch64TargetMachine::SelectFTOI(MachineInstruction *MI) {
  assert(MI->GetOperandsNumber() == 2 && "FTOI must have 2 operands");

  ExtendRegSize(MI->GetOperand(0));

  MI->SetOpcode(FCVTZS_rr);
  return true;
}

bool AArch64TargetMachine::SelectSEXT(MachineInstruction *MI) {
  assert(MI->GetOperandsNumber() == 2 && "SEXT must have 2 operands");

  ExtendRegSize(MI->GetOperand(0));

  if (MI->GetOperand(1)->IsImmediate()) {
    MI->SetOpcode(MOV_rc);
    return true;
  } else if (MI->GetOperand(1)->GetType().GetBitWidth() == 8) {
    MI->SetOpcode(SXTB);
    return true;
  } else if (MI->GetOperand(1)->GetType().GetBitWidth() == 16) {
    MI->SetOpcode(SXTH);
    return true;
  } else if (MI->GetOperand(1)->GetType().GetBitWidth() == 32) {
    MI->SetOpcode(SXTW);
    return true;
  }

  assert(!"Unimplemented!");
  return false;
}

bool AArch64TargetMachine::SelectZEXT(MachineInstruction *MI) {
  assert(MI->GetOperandsNumber() == 2 && "ZEXT must have 2 operands");

  ExtendRegSize(MI->GetOperand(0));

  if (MI->GetOperand(1)->IsImmediate()) {
    MI->SetOpcode(MOV_rc);
    return true;
  } else if (MI->GetOperand(1)->GetType().GetBitWidth() == 32) {
    MI->SetOpcode(UXTW);
    return true;
  } else if (MI->GetOperand(1)->GetType().GetBitWidth() == 8) {
    MI->SetOpcode(UXTB);
    return true;
  } else if (MI->GetOperand(1)->GetType().GetBitWidth() == 16) {
    MI->SetOpcode(UXTH);
    return true;
  } else if (MI->GetOperand(1)->GetType().GetBitWidth() == 64) {
    MI->SetOpcode(MOV_rr);
    return true;
  }

  assert(!"Unimplemented!");
  return false;
}

bool AArch64TargetMachine::SelectTRUNC(MachineInstruction *MI) {
  assert(MI->GetOperandsNumber() == 2 && "TRUNC must have 2 operands");

  if (MI->GetOperand(0)->GetType().GetBitWidth() == 8) {
    // if the operand is an immediate
    if (MI->GetOperand(1)->IsImmediate()) {
      // then calculate the truncated immediate value and issue a MOV
      int64_t ResultImm = MI->GetOperand(1)->GetImmediate() & 0xFFu;
      MI->GetOperand(1)->SetValue(ResultImm);
      MI->SetOpcode(MOV_rc);
    } else { // else issue an AND with the mask of 0xFF
      MI->SetOpcode(AND_rri);
      MI->AddImmediate(0xFFu);
    }
    // For now set the result's bitwidth to 32 if its less than that, otherwise
    // no register could be selected for it.
    // FIXME: Enforce this in the legalizer maybe (check LLVM for clues)
    ExtendRegSize(MI->GetOperand(0));
    return true;
  } else if (MI->GetOperand(0)->GetType().GetBitWidth() == 16) {
    // if the operand is an immediate
    if (MI->GetOperand(1)->IsImmediate()) {
      // then calculate the truncated immediate value and issue a MOV
      int64_t ResultImm = MI->GetOperand(1)->GetImmediate() & 0xFFFFu;
      MI->GetOperand(1)->SetValue(ResultImm);
      MI->SetOpcode(MOV_rc);
    } else { // else issue an AND with the mask of 0xFF
      MI->SetOpcode(AND_rri);
      MI->AddImmediate(0xFFFFu);
    }
    // For now set the result's bitwidth to 32 if its less than that, otherwise
    // no register could be selected for it.
    // FIXME: Enforce this in the legalizer maybe (check LLVM for clues)
    ExtendRegSize(MI->GetOperand(0));
    return true;
  }

  // in cases like
  //      TRUNC  %dst(s32), %src(s64)
  // for arm only a "mov" instruction is needed, but for $src the W subregister
  // of the X register should be used, this will be enforced in a later pass
  if (MI->GetOperand(0)->GetType().GetBitWidth() == 32 &&
      MI->GetOperand(1)->GetType().GetBitWidth() == 64) {
    if (!MI->GetOperand(1)->IsImmediate()) {
      MI->SetOpcode(MOV_rr);
      return true;
    }
  }

  assert(!"Unimplemented!");
  return false;
}

bool AArch64TargetMachine::SelectZEXT_LOAD(MachineInstruction *MI) {
  assert((MI->GetOperandsNumber() == 3) && "ZEXT_LOAD must have 3 operands");

  auto SourceSize = MI->GetOperand(1)->GetType().GetBitWidth();
  MI->RemoveOperand(1);

  if (SourceSize == 8) {
    MI->SetOpcode(LDRB);
    return true;
  }

  MI->SetOpcode(LDR);
  return true;
}

bool AArch64TargetMachine::SelectLOAD_IMM(MachineInstruction *MI) {
  assert(MI->GetOperandsNumber() == 2 &&
         "LOAD_IMM must have exactly 2 operands");

  assert(MI->GetOperand(1)->IsImmediate() && "Operand #2 must be an immediate");

  int64_t imm = MI->GetOperand(1)->GetImmediate();

  ExtendRegSize(MI->GetOperand(0));

  MaterializeConstant(MI, imm, *MI->GetOperand(0), true);

  return true;
}

bool AArch64TargetMachine::SelectMOV(MachineInstruction *MI) {
  assert(MI->GetOperandsNumber() == 2 && "MOV must have exactly 2 operands");

  if (MI->GetOperand(1)->IsImmediate()) {
    assert(IsInt<16>(MI->GetOperand(1)->GetImmediate()) &&
           "Invalid immediate value");
    MI->SetOpcode(MOV_rc);
  } else
    MI->SetOpcode(MOV_rr);

  return true;
}

bool AArch64TargetMachine::SelectMOVF(MachineInstruction *MI) {
  assert(MI->GetOperandsNumber() == 2 && "MOVF must have exactly 2 operands");

  if (MI->GetOperand(1)->IsImmediate()) {
    MI->SetOpcode(FMOV_ri);
  } else
    MI->SetOpcode(FMOV_rr);

  return true;
}

bool AArch64TargetMachine::SelectLOAD(MachineInstruction *MI) {
  assert((MI->GetOperandsNumber() == 2 || MI->GetOperandsNumber() == 3) &&
         "LOAD must have 2 or 3 operands");

  if (MI->GetOperand(0)->GetType().GetBitWidth() == 8 &&
      !MI->GetOperand(0)->GetType().IsPointer()) {
    MI->SetOpcode(LDRB);
    if (MI->GetOperand(0)->GetSize() < 32)
      MI->GetOperand(0)->GetTypeRef().SetBitWidth(32);
    return true;
  }

  if (MI->GetOperand(1)->IsStackAccess()) {
    auto StackSlotID = MI->GetOperand(1)->GetSlot();
    auto ParentFunc = MI->GetParent()->GetParent();
    auto Size = ParentFunc->GetStackObjectSize(StackSlotID);
    switch (Size) {
    case 1:
      MI->SetOpcode(LDRB);
      if (MI->GetOperand(0)->GetSize() < 32)
        MI->GetOperand(0)->GetTypeRef().SetBitWidth(32);
      return true;
    case 2:
      MI->SetOpcode(LDRH);
      if (MI->GetOperand(0)->GetSize() < 32)
        MI->GetOperand(0)->GetTypeRef().SetBitWidth(32);
      return true;
    case 4:
      MI->SetOpcode(LDR);
      return true;
      ;
    default:
      break;
    }
  }

  MI->SetOpcode(LDR);
  return true;
}

bool AArch64TargetMachine::SelectSTORE(MachineInstruction *MI) {
  assert((MI->GetOperandsNumber() == 2 || MI->GetOperandsNumber() == 3) &&
         "STORE must have 2 or 3 operands");

  MachineFunction *ParentMF = nullptr;
  if (MI->GetOperandsNumber() == 2)
    ParentMF = MI->GetParent()->GetParent();
  auto Op0 = MI->GetOperand(0);
  auto OpLast = MI->GetOperand(MI->GetOperandsNumber() - 1);

  if (OpLast->GetType().GetBitWidth() == 8 ||
      (MI->GetOperandsNumber() == 2 && ParentMF->IsStackSlot(Op0->GetSlot()) &&
       ParentMF->GetStackObjectSize(Op0->GetSlot()) == 1)) {
    MI->SetOpcode(STRB);
  } else if (OpLast->GetType().GetBitWidth() == 16 ||
      (MI->GetOperandsNumber() == 2 && ParentMF->IsStackSlot(Op0->GetSlot()) &&
       ParentMF->GetStackObjectSize(Op0->GetSlot()) == 2)) {
    MI->SetOpcode(STRH);
  } else
    MI->SetOpcode(STR);

  ExtendRegSize(MI->GetOperand(1));
  return true;
}

bool AArch64TargetMachine::SelectSTACK_ADDRESS(MachineInstruction *MI) {
  assert(MI->GetOperandsNumber() == 2 && "STACK_ADDRESS must have 2 operands");

  MI->SetOpcode(ADD_rri);
  return true;
}

bool AArch64TargetMachine::SelectBRANCH(MachineInstruction *MI) {
  // 1) Get the preceding instruction if exists
  // 2) If a compare then use its condition to determine the condition code
  //    for this branch
  // FIXME: not sure if for a branch it is REQUIRED to have a compare before
  //        it or its just optional (likely its optional)
  auto &BBInstructions = MI->GetParent()->GetInstructions();
  MachineInstruction *PrecedingMI = nullptr;

  for (size_t i = 0; i < BBInstructions.size(); i++)
    // find the current instruction index
    if (&BBInstructions[i] == MI && i > 0) {
      PrecedingMI = &BBInstructions[i - 1];
      break;
    }

  if (MI->IsFallThroughBranch()) {
    assert(PrecedingMI && "For now assume a preceding cmp instruction");

    // choose the appropriate conditional branch based on the cmp type
    switch (PrecedingMI->GetRelation()) {
    case MachineInstruction::EQ:
      MI->SetOpcode(BEQ);
      break;
    case MachineInstruction::NE:
      MI->SetOpcode(BNE);
      break;
    case MachineInstruction::LE:
      MI->SetOpcode(BLE);
      break;
    case MachineInstruction::LT:
      MI->SetOpcode(BLT);
      break;
    case MachineInstruction::GE:
      MI->SetOpcode(BGE);
      break;
    case MachineInstruction::GT:
      MI->SetOpcode(BGT);
      break;
    default:
      // if the preceding instruction is not a compare, then simply check for
      // equality
      MI->SetOpcode(BEQ);
      break;
    }

    MI->RemoveOperand(0);
    return true;
  }

  return false;
}

bool AArch64TargetMachine::SelectJUMP(MachineInstruction *MI) {
  MI->SetOpcode(B);
  return true;
}

bool AArch64TargetMachine::SelectCALL(MachineInstruction *MI) {
  MI->SetOpcode(BL);
  return true;
}

bool AArch64TargetMachine::SelectRET(MachineInstruction *MI) {
  MI->SetOpcode(RET);
  return true;
}
