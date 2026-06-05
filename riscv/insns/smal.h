require_extension(EXT_ZPSFOPERAND);
if (xlen == 32) {
  __int128 res = (__int128)(sreg_t)RS1_PAIR
               + (__int128)((sreg_t)P_SH(RS2, 0) * (sreg_t)P_SH(RS2, 1));
  WRITE_RD_PAIR((sreg_t)res);
} else {
  __int128 res = (__int128)(sreg_t)RS1
               + (__int128)((sreg_t)P_SH(RS2, 0) * (sreg_t)P_SH(RS2, 1))
               + (__int128)((sreg_t)P_SH(RS2, 2) * (sreg_t)P_SH(RS2, 3));
  WRITE_RD((sreg_t)res);
}
