#include "../backend/AssemblyEmitter.hpp"
#include "../backend/LLIROptimizer.hpp"
#include "../backend/IRtoLLIR.hpp"
#include "../backend/InsturctionSelection.hpp"
#include "../backend/MachineInstructionLegalizer.hpp"
#include "../backend/PrologueEpilogInsertion.hpp"
#include "../backend/RegisterAllocator.hpp"
#include "../backend/RegisterClassSelection.hpp"
#include "../backend/TargetArchs/AArch64/AArch64TargetMachine.hpp"
#include "../backend/TargetArchs/AArch64/AArch64XRegToWRegFixPass.hpp"
#include "../backend/TargetArchs/RISCV/RISCVTargetMachine.hpp"
#include "../middle_end/IR/IRFactory.hpp"
#include "../middle_end/Transforms/PassManager.hpp"
#include "ErrorLogger.hpp"
#include "ast/ASTPrint.hpp"
#include "ast/Semantics.hpp"
#include "lexer/Lexer.hpp"
#include "parser/Parser.hpp"
#include "preprocessor/PreProcessor.hpp"
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

/*
 * It will iterate through all the lines in file and
 * put them in given vector
 */
bool getFileContent(const std::string &fileName,
                    std::vector<std::string> &vecOfStrs) {
  // Open the File
  std::ifstream in(fileName.c_str());
  // Check if object is valid
  if (!in) {
    std::cerr << "Cannot open the File : " << fileName << std::endl;
    return false;
  }
  std::string str;
  // Read the next line from File until it reaches the end.
  while (std::getline(in, str)) {
    // Line contains string of length > 0 then save it in vector
    vecOfStrs.push_back(str);
  }
  // Close The File
  in.close();
  return true;
}

/// TODO: Make a proper driver
int main(int argc, char *argv[]) {
  std::string FilePath = "tests/test.txt";
  bool DumpPreProcessedFile = false;
  bool DumpTokens = false;
  bool DumpAST = false;
  bool DumpIR = false;
  bool PrintBeforePasses = false;
  bool Wall = false;
  std::set<Optimization> RequestedOptimizations;
  bool RunLLIROpt = false;
  std::string TargetArch = "aarch64";

  for (int i = 0; i < argc; i++)
    if (argv[i][0] != '-')
      FilePath = std::string(argv[i]);
    else {
      if (!std::string(&argv[i][1]).compare("llir-opt")) {
        RunLLIROpt = true;
        continue;
      } else if (!std::string(&argv[i][1]).compare("copy-propagation")) {
        RequestedOptimizations.insert(Optimization::CopyPropagation);
        continue;
      } else if (!std::string(&argv[i][1]).compare("cse")) {
        RequestedOptimizations.insert(Optimization::CopyPropagation);
        RequestedOptimizations.insert(Optimization::CSE);
        continue;
      } else if (!std::string(&argv[i][1]).compare("O")) {
        RequestedOptimizations.insert(Optimization::CopyPropagation);
        RequestedOptimizations.insert(Optimization::CSE);
        continue;
      } else if (!std::string(&argv[i][1]).compare("E")) {
        DumpPreProcessedFile = true;
        continue;
      } else if (!std::string(&argv[i][1]).compare("Wall")) {
        Wall = true;
        continue;
      } else if (!std::string(&argv[i][1]).compare("dump-tokens")) {
        DumpTokens = true;
        continue;
      } else if (!std::string(&argv[i][1]).compare("dump-ast")) {
        DumpAST = true;
        continue;
      } else if (!std::string(&argv[i][1]).compare("dump-ir")) {
        DumpIR = true;
        continue;
      } else if (!std::string(&argv[i][1]).compare("print-before-passes")) {
        PrintBeforePasses = true;
        continue;
      } else if (!std::string(&argv[i][1]).compare(0, 5, "arch=")) {
        TargetArch = std::string(&argv[i][6]);
        continue;
      } else {
        std::cerr << "Error: Unknown argument '" << argv[i] << "'" << std::endl;
        return -1;
      }
    }

  if (DumpTokens) {
    std::vector<std::string> src;
    getFileContent(FilePath.c_str(), src);

    Lexer lexer(src);

    auto t1 = lexer.Lex();
    while (t1.GetKind() != Token::EndOfFile && t1.GetKind() != Token::Invalid) {
      std::cout << t1.ToString() << std::endl;
      t1 = lexer.Lex();
    }
  }

  std::vector<std::string> src;
  auto Success = getFileContent(FilePath.c_str(), src);
  assert(Success && "Unable to open file");

  PreProcessor(src, FilePath).Run();

  if (DumpPreProcessedFile) {
    for (auto &Line : src)
      std::cout << Line << std::endl;
    std::cout << std::endl;
  }

  std::unique_ptr<TargetMachine> TM;

  if (TargetArch == "riscv32")
    TM = std::make_unique<RISCV::RISCVTargetMachine>();
  else
    TM = std::make_unique<AArch64::AArch64TargetMachine>();

  Module IRModule;
  IRFactory IRF(IRModule, TM.get());
  ErrorLogger ErrorLog(FilePath, src);
  Parser parser(src, &IRF, ErrorLog);
  auto AST = parser.Parse();

  if (ErrorLog.HasErrors(Wall)) {
    ErrorLog.ReportErrors();
    exit(1);
  }

  if (DumpAST) {
    auto AstPrinter = std::make_unique<ASTPrint>();
    AST->Accept(AstPrinter.get());
  }

  // Do semantic analysis on the AST
  auto Sema = std::make_unique<Semantics>(ErrorLog);
  AST->Accept(Sema.get());

  if (ErrorLog.HasErrors(Wall)) {
    ErrorLog.ReportErrors();
    exit(1);
  }

  AST->IRCodegen(&IRF);

  const bool Optimize = !RequestedOptimizations.empty();
  if (Optimize) {
    PassManager PM(&IRModule, RequestedOptimizations);
    PM.RunAll();
  }

  if (DumpIR)
    IRModule.Print();

  MachineIRModule LLIRModule;
  IRtoLLIR I2LLIR(IRModule, &LLIRModule, TM.get());
  I2LLIR.GenerateLLIRFromIR();

  if (PrintBeforePasses) {
    if (RunLLIROpt)
      std::cout << "<<<<< Before LLIR Optimizer >>>>>" << std::endl
                << std::endl;
    else
      std::cout << "<<<<< Before Legalizer >>>>>" << std::endl << std::endl;
    LLIRModule.Print(TM.get());
    std::cout << std::endl;
  }

  if (RunLLIROpt) {
    LLIROptimizer LLIROpt(&LLIRModule, TM.get());
    LLIROpt.Run();

    if (PrintBeforePasses) {
      std::cout << "<<<<< Before Legalizer >>>>>" << std::endl << std::endl;
      LLIRModule.Print(TM.get());
      std::cout << std::endl;
    }
  }

  MachineInstructionLegalizer Legalizer(&LLIRModule, TM.get());
  Legalizer.Run();

  if (PrintBeforePasses) {
    std::cout << "<<<<< Before Register Class Selection >>>>>" << std::endl
              << std::endl;
    LLIRModule.Print(TM.get());
    std::cout << std::endl;
  }

  RegisterClassSelection RCS(&LLIRModule, TM.get());
  RCS.Run();

  if (PrintBeforePasses) {
    std::cout << "<<<<< Before Instruction Selection >>>>>" << std::endl
              << std::endl;
    LLIRModule.Print(TM.get());
    std::cout << std::endl;
  }

  InsturctionSelection IS(&LLIRModule, TM.get());
  IS.InstrSelect();

  if (PrintBeforePasses) {
    std::cout << "<<<<< Before Register Allocation >>>>>" << std::endl
              << std::endl;
    LLIRModule.Print(TM.get());
    std::cout << std::endl;
  }

  RegisterAllocator RA(&LLIRModule, TM.get());
  RA.RunRA();

  if (PrintBeforePasses) {
    std::cout << "<<<<< Before Prologue/Epilog Insertion >>>>>" << std::endl
              << std::endl;
    LLIRModule.Print(TM.get());
    std::cout << std::endl;
  }
  PrologueEpilogInsertion PEI(&LLIRModule, TM.get());
  PEI.Run();

  if (TargetArch == "aarch64")
    AArch64XRegToWRegFixPass(&LLIRModule, TM.get()).Run();

  if (PrintBeforePasses) {
    std::cout << "<<<<< Before Emitting Assembly >>>>>" << std::endl
              << std::endl;
    LLIRModule.Print(TM.get());
    std::cout << std::endl;
  }

  AssemblyEmitter AE(&LLIRModule, TM.get());
  AE.GenerateAssembly();

  return 0;
}
