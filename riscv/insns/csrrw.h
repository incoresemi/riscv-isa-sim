int csr = validate_csr(insn.csr(), true);
reg_t old_csr = p->get_csr(csr, insn, true);
reg_t old_rs1 = RS1;
WRITE_RD(sext_xlen(old_csr));
p->put_csr(csr, old_rs1);
serialize();
