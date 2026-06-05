require_vector_vs;
P_LOOP(8, {
  // bool sat = false;
  // pd = (sat_add<int8_t, uint8_t>(ps1, ps2, sat));
  // P_SET_OV(sat);
  int16_t sum = (int16_t)ps1 + (int16_t)ps2;
  bool sat = false;
  if (sum > INT8_MAX) {
    pd = INT8_MAX;
    sat = true;
  } else if (sum < INT8_MIN) {
    pd = INT8_MIN;
    sat = true;
  } else {
    pd = (int8_t)sum;
  }
  P_SET_OV(sat);
})
