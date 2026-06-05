P_LOOP(8, {
  // pd = (ps1 - ps2) >> 1;
  pd = (int8_t)(((int16_t)ps1 - (int16_t)ps2) >> 1);
})
