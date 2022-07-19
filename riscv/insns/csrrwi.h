int csr = validate_csr(insn.csr(), true);
reg_t old_csr = p->get_csr(csr, insn, true);
WRITE_RD(sext_xlen(old_csr));
p->put_csr(csr, insn.rs1());
serialize();
