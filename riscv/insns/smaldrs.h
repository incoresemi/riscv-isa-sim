P_64_PROFILE_REDUCTION(16, {
  __int128 prod = (__int128)ps1 * (__int128)ps2;
  if (i & 1) {
    rd = (sreg_t)((__int128)rd - prod);
  } else {
    rd = (sreg_t)((__int128)rd + prod);
  }
})
