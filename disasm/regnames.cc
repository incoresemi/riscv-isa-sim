// See LICENSE for license details.

#include "disasm.h"

const char* xpr_name[] = {
  "zero", "ra", "sp",  "gp",  "tp", "t0",  "t1",  "t2",
  "s0",   "s1", "a0",  "a1",  "a2", "a3",  "a4",  "a5",
  "a6",   "a7", "s2",  "s3",  "s4", "s5",  "s6",  "s7",
  "s8",   "s9", "s10", "s11", "t3", "t4",  "t5",  "t6"
};

const char* xpr_numeric[] = {
  "x0",  "x1",  "x2",  "x3",  "x4",  "x5",  "x6",  "x7",
  "x8",  "x9",  "x10", "x11", "x12", "x13", "x14", "x15",
  "x16", "x17", "x18", "x19", "x20", "x21", "x22", "x23",
  "x24", "x25", "x26", "x27", "x28", "x29", "x30", "x31"
};

const char* get_xpr_name(int reg_num, bool numeric) {
  return numeric ? xpr_numeric[reg_num] : xpr_name[reg_num];
}

const char* fpr_numeric[] = {
  "f0",  "f1",  "f2",  "f3",  "f4",  "f5",  "f6",  "f7",
  "f8",  "f9",  "f10", "f11", "f12", "f13", "f14", "f15",
  "f16", "f17", "f18", "f19", "f20", "f21", "f22", "f23",
  "f24", "f25", "f26", "f27", "f28", "f29", "f30", "f31"
};

const char* get_fpr_name(int reg_num, bool numeric) {
  return numeric ? fpr_numeric[reg_num] : fpr_name[reg_num];
}

const char* vr_numeric[] = {
  "v0",  "v1",  "v2",  "v3",  "v4",  "v5",  "v6",  "v7",
  "v8",  "v9",  "v10", "v11", "v12", "v13", "v14", "v15",
  "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
  "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31"
};

const char* get_vr_name(int reg_num, bool numeric) {
  return numeric ? vr_numeric[reg_num] : vr_name[reg_num];
}

const char* fpr_name[] = {
  "ft0", "ft1", "ft2",  "ft3",  "ft4", "ft5", "ft6",  "ft7",
  "fs0", "fs1", "fa0",  "fa1",  "fa2", "fa3", "fa4",  "fa5",
  "fa6", "fa7", "fs2",  "fs3",  "fs4", "fs5", "fs6",  "fs7",
  "fs8", "fs9", "fs10", "fs11", "ft8", "ft9", "ft10", "ft11"
};

const char* vr_name[] = {
  "v0",  "v1",  "v2",  "v3",  "v4",  "v5",  "v6",  "v7",
  "v8",  "v9",  "v10", "v11", "v12", "v13", "v14", "v15",
  "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
  "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31"
};

// struct : public arg_t {
//   std::string to_string(insn_t insn) const {
//     if (((disassembler_t*)this)->numeric_reg_names) {
//       return "x" + std::to_string(insn.rd());
//     }
//     return xpr_name[insn.rd()];
//   }
// } xrd;

// struct : public arg_t {
//   std::string to_string(insn_t insn) const {
//     if (((disassembler_t*)this)->numeric_reg_names) {
//       return "x" + std::to_string(insn.rs1());
//     }
//     return xpr_name[insn.rs1()];
//   }
// } xrs1;

// struct : public arg_t {
//   std::string to_string(insn_t insn) const {
//     if (((disassembler_t*)this)->numeric_reg_names) {
//       return "x" + std::to_string(insn.rs2());
//     }
//     return xpr_name[insn.rs2()];
//   }
// } xrs2;

// struct : public arg_t {
//   std::string to_string(insn_t insn) const {
//     if (((disassembler_t*)this)->numeric_reg_names) {
//       return "x" + std::to_string(insn.rs3());
//     }
//     return xpr_name[insn.rs3()];
//   }
// } xrs3;

const char* csr_name(int which) {
  switch (which) {
    #define DECLARE_CSR(name, number)  case number: return #name;
    #include "encoding.h"
    #undef DECLARE_CSR
  }
  return "unknown-csr";
}
