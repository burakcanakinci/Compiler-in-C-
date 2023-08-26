#ifndef MACHINE_OPERAND_HPP
#define MACHINE_OPERAND_HPP

#include "LowLevelType.hpp"
#include "TargetRegister.hpp"
#include <cstdint>
#include <iostream>

class TargetMachine;

class MachineOperand {
public:
  enum MOKind : unsigned {
    NONE,
    REGISTER,
    INT_IMMEDIATE,
    FP_IMMEDIATE,
    MEMORY_ADDRESS,
    STACK_ACCESS,
    PARAMETER,
    LABEL,
    FUNCTION_NAME,
    GLOBAL_SYMBOL,
  };

  MachineOperand() {}

  void SetToVirtualRegister() { Type = REGISTER; Virtual = true; }
  void SetToRegister() { Type = REGISTER; Virtual = false; }
  void SetToIntImm() { Type = INT_IMMEDIATE; }
  void SetToFPImm() { Type = FP_IMMEDIATE; }
  void SetToMemAddr() { Type = MEMORY_ADDRESS; Virtual = true; }
  void SetToStackAccess() { Type = STACK_ACCESS; }
  void SetToParameter() { Type = PARAMETER; }
  void SetToLabel() { Type = LABEL; }
  void SetToFunctionName() { Type = FUNCTION_NAME; }
  void SetToGlobalSymbol() { Type = GLOBAL_SYMBOL; }

  int64_t GetImmediate() const { return IntVal; }
  double GetFPImmediate() const { return FloatVal; }
  int64_t GetReg() const { return IntVal; }
  uint64_t GetSlot() const { return IntVal; }
  void SetReg(uint64_t V) { SetValue(V); }
  void SetValue(uint64_t V) { IntVal = V; }
  void SetFPValue(double V) { FloatVal = V; }

  void SetOffset(int o) { Offset = o; }
  int GetOffset() const { return Offset; }

  void SetRegClass(unsigned rc) { RegisterClass = rc; }
  unsigned GetRegClass() const { return RegisterClass; }

  void SetType(LowLevelType LLT) { this->LLT = LLT; }
  LowLevelType GetType() const { return LLT; }
  LowLevelType &GetTypeRef() { return LLT; }

  const char *GetLabel() { return Label; }
  const char *GetFunctionName() { return Label; }
  std::string &GetGlobalSymbol() { return GlobalSymbol; }
  void SetLabel(const char *L) { Label = L; }
  void SetGlobalSymbol(const std::string &GS) { GlobalSymbol = GS; }

  bool IsVirtual() const { return Virtual; }
  void SetVirtual(bool v) { Virtual = v; }

  /// Returns true if the operand is a physical register
  bool IsRegister() const { return Type == REGISTER && !Virtual; }
  bool IsVirtualReg() const { return Type == REGISTER && Virtual; }
  bool IsImmediate() const { return Type == INT_IMMEDIATE || Type == FP_IMMEDIATE; }
  bool IsFPImmediate() const { return Type == FP_IMMEDIATE; }
  bool IsMemory() const { return Type == MEMORY_ADDRESS; }
  bool IsStackAccess() const { return Type == STACK_ACCESS; }
  bool IsParameter() const { return Type == PARAMETER; }
  bool IsLabel() const { return Type == LABEL; }
  bool IsFunctionName() const { return Type == FUNCTION_NAME; }
  bool IsGlobalSymbol() const { return Type == GLOBAL_SYMBOL; }


  unsigned GetSize() const { return LLT.GetBitWidth(); }
  void SetSize(unsigned s) { return LLT.SetBitWidth(s); }

  /// To be able to use this class in a set
  bool operator<(const MachineOperand& rhs) const {
    return IntVal < rhs.IntVal;
  }

  static MachineOperand CreateRegister(uint64_t Reg, unsigned BitWidth = 32) {
    MachineOperand MO;
    MO.SetToRegister();
    MO.SetReg(Reg);
    MO.SetType(LowLevelType::CreateScalar(BitWidth));
    return MO;
  }

  static MachineOperand CreateVirtualRegister(uint64_t Reg, unsigned BitWidth = 32) {
    MachineOperand MO;
    MO.SetToVirtualRegister();
    MO.SetReg(Reg);
    MO.SetType(LowLevelType::CreateScalar(BitWidth));
    return MO;
  }

  static MachineOperand CreateImmediate(uint64_t Val, unsigned BitWidth = 32) {
    MachineOperand MO;
    MO.SetToIntImm();
    MO.SetValue(Val);
    MO.SetType(LowLevelType::CreateScalar(BitWidth));
    return MO;
  }

  static MachineOperand CreateFPImmediate(double Val, unsigned BitWidth = 32) {
    MachineOperand MO;
    MO.SetToFPImm();
    MO.SetFPValue(Val);
    MO.SetType(LowLevelType::CreateScalar(BitWidth));
    return MO;
  }

  static MachineOperand CreateMemory(uint64_t Id, unsigned BitWidth = 32) {
    MachineOperand MO;
    MO.SetToMemAddr();
    MO.SetValue(Id);
    MO.SetType(LowLevelType::CreatePTR(BitWidth));
    return MO;
  }

  static MachineOperand CreateMemory(uint64_t Id, int Offset,
                                     unsigned BitWidth) {
    MachineOperand MO;
    MO.SetToMemAddr();
    MO.SetOffset(Offset);
    MO.SetValue(Id);
    MO.SetType(LowLevelType::CreatePTR(BitWidth));
    return MO;
  }

  static MachineOperand CreateStackAccess(uint64_t Slot, int Offset = 0) {
    MachineOperand MO;
    MO.SetToStackAccess();
    MO.SetOffset(Offset);
    MO.SetValue(Slot);
    return MO;
  }

  static MachineOperand CreateParameter(uint64_t Val) {
    MachineOperand MO;
    MO.SetToParameter();
    MO.SetReg(Val);
    return MO;
  }

  static MachineOperand CreateGlobalSymbol(std::string &Symbol) {
    MachineOperand MO;
    MO.SetToGlobalSymbol();
    MO.SetGlobalSymbol(Symbol);
    return MO;
  }

  static MachineOperand CreateLabel(const char* Label) {
    MachineOperand MO;
    MO.SetToLabel();
    MO.SetLabel(Label);
    return MO;
  }

  static MachineOperand CreateFunctionName(const char* Label) {
    MachineOperand MO;
    MO.SetToFunctionName();
    MO.SetLabel(Label);
    return MO;
  }

  bool operator==(const MachineOperand &RHS) {
    if (Type != RHS.Type)
      return false;

    // TODO: maybe check equality of LLT, although for registers
    // it most likely should not needed, but maybe for immediate it does

    switch (Type) {
    case REGISTER:
      return (IntVal == RHS.IntVal && Virtual == RHS.Virtual &&
              RegisterClass == RHS.RegisterClass);
    case INT_IMMEDIATE:
      return IntVal == RHS.IntVal;
    case FP_IMMEDIATE:
      return FloatVal == RHS.FloatVal;
    case MEMORY_ADDRESS:
    case STACK_ACCESS:
      return (IntVal == RHS.IntVal && Virtual == RHS.Virtual &&
              RegisterClass == RHS.RegisterClass && Offset == RHS.Offset);
    case PARAMETER:
      return false; // TODO
    case LABEL:
    case FUNCTION_NAME:
      return std::string(Label) == std::string(RHS.Label);

    case GLOBAL_SYMBOL:
      return GlobalSymbol == RHS.GlobalSymbol;
    default:
      assert(!"Unreachable");
    }
  }

  bool operator!=(const MachineOperand &RHS) {
    return !(*this == RHS);
  }

  void Print(TargetMachine *TM) const;

private:
  unsigned Type = NONE;
  union {
    uint64_t IntVal;
    double FloatVal;
    const char *Label;
  };
  int Offset = 0;
  LowLevelType LLT;
  std::string GlobalSymbol;
  bool Virtual = false;
  unsigned RegisterClass = ~0;
};

#endif
