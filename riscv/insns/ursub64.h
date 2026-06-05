P_64_UPROFILE({
  reg_t diff = rs1 - rs2;
  reg_t borrow = (rs2 > rs1) ? 1 : 0;
  rd = (diff >> 1) | (borrow << 63);
})
