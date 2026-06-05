P_64_PROFILE_REDUCTION(32, {
  __int128 p0 = (__int128)(sreg_t)P_SH(ps1, 0) * (sreg_t)P_SH(ps2, 1);
  __int128 p1 = (__int128)(sreg_t)P_SH(ps1, 1) * (sreg_t)P_SH(ps2, 0);
  rd = (sreg_t)((__int128)rd + p0 + p1);
})
