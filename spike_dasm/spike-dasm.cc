// See LICENSE for license details.

// This little program finds occurrences of strings like
//  DASM(ffabc013)
// in its input, then replaces them with the disassembly
// enclosed hexadecimal number, interpreted as a RISC-V
// instruction.

#include "config.h"
#include "disasm.h"
#include "extension.h"
#include <iostream>
#include <string>
#include <cstdint>
#include <fesvr/option_parser.h>
using namespace std;

int main(int UNUSED argc, char** argv)
{
  string s;
  const char* isa = DEFAULT_ISA;
  bool numeric_regs = false;
  bool help = false;

  const char* help_text = 
    "Usage: spike-dasm [options] < input_file\n"
    "\n"
    "Options:\n"
    "  --help            Show this help message\n"
    "  --isa=<str>      Set the ISA string (default: " DEFAULT_ISA ")\n"
    "  --numeric-reg    Use numeric register names (x0-x31) instead of ABI names\n"
#ifdef HAVE_DLOPEN
    "  --extension=<so> Load extension library\n"
#endif
    "\n"
    "Description:\n"
    "  spike-dasm disassembles RISC-V instructions. It reads from standard input\n"
    "  and looks for patterns like DASM(instruction_hex). Each such pattern is\n"
    "  replaced with the disassembly of the instruction.\n"
    "\n"
    "Example:\n"
    "  echo 'DASM(00a58533)' | spike-dasm\n"
    "  DASM(00a58533) -> add a0, a1, a0\n";

  std::function<extension_t*()> extension;
  option_parser_t parser;
  parser.option(0, "help", 0, [&](const char* s){help = true;});
#ifdef HAVE_DLOPEN
  parser.option(0, "extension", 1, [&](const char* s){extension = find_extension(s);});
#endif
  parser.option(0, "isa", 1, [&](const char* s){isa = s;});
  parser.option(0, "numeric-reg", 0, [&](const char* s){numeric_regs = true;});
  parser.parse(argv);

  if (help) {
    std::cerr << help_text;
    return 0;
  }

  isa_parser_t isa_parser(isa, DEFAULT_PRIV);
  disassembler_t* disassembler = new disassembler_t(&isa_parser);
  if (extension) {
    for (auto disasm_insn : extension()->get_disasms()) {
      disassembler->add_insn(disasm_insn);
    }
  }
  disassembler->numeric_reg_names = numeric_regs;

  while (getline(cin, s))
  {
    for (size_t pos = 0; (pos = s.find("DASM(", pos)) != string::npos; )
    {
      size_t start = pos;

      pos += strlen("DASM(");

      if (s[pos] == '0' && (s[pos+1] == 'x' || s[pos+1] == 'X'))
        pos += 2;

      if (!isxdigit(s[pos]))
        continue;

      char* endp;
      insn_bits_t bits = strtoull(&s[pos], &endp, 16);
      if (*endp != ')')
        continue;

      string dis = disassembler->disassemble(bits);
      s = s.substr(0, start) + dis + s.substr(endp - &s[0] + 1);
      pos = start + dis.length();
    }

    cout << s << '\n';
  }

  return 0;
}
