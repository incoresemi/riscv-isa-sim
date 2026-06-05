require_vector_vs;
P_64_PROFILE({
  /*
  bool sat = false;
  rd = (sat_add<int64_t, uint64_t>(rs1, rs2, sat));
  P_SET_OV(sat);
  */
  __int128 sum = (__int128)rs1 + (__int128)rs2;
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
})
