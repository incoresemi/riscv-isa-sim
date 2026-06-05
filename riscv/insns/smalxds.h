P_64_PROFILE_REDUCTION(32, {
  __int128 p_add = (__int128)(sreg_t)P_SH(ps1, 1) * (sreg_t)P_SH(ps2, 0);
  __int128 p_sub = (__int128)(sreg_t)P_SH(ps1, 0) * (sreg_t)P_SH(ps2, 1);
  rd = (sreg_t)((__int128)rd + p_add - p_sub);
})
