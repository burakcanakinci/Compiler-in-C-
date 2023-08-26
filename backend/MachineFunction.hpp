#ifndef MACHINEFUNCTION_HPP
#define MACHINEFUNCTION_HPP

#include "LowLevelType.hpp"
#include "MachineBasicBlock.hpp"
#include "StackFrame.hpp"
#include <vector>

class Function;

class MachineFunction {
  using BasicBlockList = std::vector<MachineBasicBlock>;
  /// #1: nth param
  /// #2: its low level type
  /// #3: is it an implicit struct pointer?
  /// #4: is it a floating point value?
  using ParamList = std::vector<std::tuple<unsigned, LowLevelType, bool, bool>>;
  using PhysRegList = std::vector<unsigned>;

public:
  MachineFunction() {}
  MachineFunction(BasicBlockList BBs) : BasicBlocks(BBs) {}

  void SetBasicBlocks(BasicBlockList BBs) { BasicBlocks = BBs; }
  BasicBlockList &GetBasicBlocks() { return BasicBlocks; }

  std::string &GetName() { return Name; }
  void SetName(std::string &Name) { this->Name = Name; }

  unsigned GetNextVReg() const { return NextVReg; }
  void SetNextVReg(const unsigned r) { NextVReg = r; }

  void InsertStackSlot(unsigned ID, unsigned Size, unsigned Align = 4) {
    if (NextVReg <= ID)
      NextVReg = ID + 1;
    SF.InsertStackSlot(ID, Size, Align);
  }

  void InsertParameter(unsigned ID, LowLevelType LLT,
                       bool ImplStructPtr = false, bool IsFP = false) {
    Parameters.push_back({ID, LLT, ImplStructPtr, IsFP});
  }

  ParamList GetParameters() { return Parameters; }

  PhysRegList &GetUsedCalleSavedRegs() { return UsedCalleSavedRegs; }

  StackFrame &GetStackFrame() { return SF; }
  unsigned GetStackFrameSize() { return SF.GetSize(); }
  unsigned GetStackObjectPosition(unsigned ID) { return SF.GetPosition(ID); }
  unsigned GetStackObjectSize(unsigned ID) { return SF.GetSize(ID); }

  bool IsStackSlot(unsigned ID) { return SF.IsStackSlot(ID); }

  /// Get the next available virtual register.
  unsigned GetNextAvailableVReg();

  void SetToCaller() { HasCall = true; }
  bool IsCaller() const { return HasCall; }

  void Print(TargetMachine *TM) const;

private:
  std::string Name;
  ParamList Parameters;
  StackFrame SF;
  BasicBlockList BasicBlocks;
  unsigned NextVReg = 0;

  /// Predicate to signal if the function is calling other functions or not
  bool HasCall = false;

  /// To keep track what registers were used in the function which must be saved
  /// by the called function if clobbered. This information is used for the
  /// prolog and epilog insertion to restore these registers.
  PhysRegList UsedCalleSavedRegs;
};

#endif
