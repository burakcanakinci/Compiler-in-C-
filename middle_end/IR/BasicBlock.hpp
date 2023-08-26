#ifndef BASIC_BLOCK_HPP
#define BASIC_BLOCK_HPP

#include "Instructions.hpp"
#include "Value.hpp"
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class Function;
// class Instruction;

class BasicBlock : public Value {
public:
  using InstructionList = std::vector<std::unique_ptr<Instruction>>;

  BasicBlock(std::string Name, Function *Parent)
      : Name(std::move(Name)), Parent(Parent), Value(Value::LABEL) {}
  explicit BasicBlock(Function *Parent) : Parent(Parent), Value(Value::LABEL) {}

  /// Since unique_ptr is not copyable, therefore this class should not as well
  BasicBlock(const BasicBlock &) = delete;
  BasicBlock(BasicBlock &&) = default;

  /// Insert the @Instruction to the back of the Instructions vector.
  Instruction *Insert(std::unique_ptr<Instruction> Instruction);

  /// Inserting a StackAllocationInstruction into the entry BasicBlock. It will
  /// be Inserted before the first none SA instruction. Or into the end of the
  /// list if the Instruction list is either empty or contains only SA
  /// instructions.
  Instruction *InsertSA(std::unique_ptr<Instruction> Instruction);

  std::string &GetName() { return Name; }
  void SetName(const std::string &N) { Name = N; }

  InstructionList &GetInstructions() { return Instructions; }

  void Print() const;

private:
  std::string Name;
  InstructionList Instructions;
  Function *Parent;
};

#endif
