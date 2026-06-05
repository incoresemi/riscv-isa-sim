P_64_UPROFILE({
  __uint128_t sum = (__uint128_t)rs1 + (__uint128_t)rs2;
  rd = (reg_t)(sum >> 1);
})
