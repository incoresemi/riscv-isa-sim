bool write = insn.rs1() != 0;
int csr = validate_csr(insn.csr(), write);
reg_t old_csr = p->get_csr(csr, insn, write);
WRITE_RD(sext_xlen(old_csr));
if (write) {
  p->put_csr(csr, old_csr | insn.rs1());
}
serialize();
