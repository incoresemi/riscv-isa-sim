P_CROSS_LOOP(16, {
  pd = (int16_t)(((int32_t)ps1 - (int32_t)ps2) >> 1);
}, {
  pd = (int16_t)(((int32_t)ps1 + (int32_t)ps2) >> 1);
})
