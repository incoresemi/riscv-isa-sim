P_64_PROFILE_REDUCTION(32, {
  __int128 prod = (__int128)(sreg_t)P_SH(ps1, 0) * (sreg_t)P_SH(ps2, 1);
  rd = (sreg_t)((__int128)rd + prod);
})
