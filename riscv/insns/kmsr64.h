require_vector_vs;
P_64_PROFILE_BASE()
P_64_PROFILE_PARAM(true, false)

__int128 mres0 = -((__int128)(sreg_t)P_SW(rs1, 0) * (sreg_t)P_SW(rs2, 0));
__int128 mres1 = -((__int128)(sreg_t)P_SW(rs1, 1) * (sreg_t)P_SW(rs2, 1));
__int128 sum = (xlen == 32) ? ((__int128)rd + mres0)
                            : ((__int128)rd + mres0 + mres1);
bool sat = false;
if (sum > INT64_MAX) {
  rd = INT64_MAX;
  sat = true;
} else if (sum < INT64_MIN) {
  rd = INT64_MIN;
  sat = true;
} else {
  rd = (sreg_t)sum;
}
P_SET_OV(sat);
P_64_PROFILE_END()
