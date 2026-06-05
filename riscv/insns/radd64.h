P_64_PROFILE({
  // rd = (rs1 + rs2) >> 1;
  // if (rs1 > 0 && rs2 > 0) {
  //   rd &= ~((reg_t)1 << 63);
  // } else if (rs1 < 0 && rs2 < 0) {
  //   rd |= ((reg_t)1 << 63);
  // }
  __int128 sum = (__int128)rs1 + (__int128)rs2;
  rd = (sreg_t)(sum >> 1);
})
