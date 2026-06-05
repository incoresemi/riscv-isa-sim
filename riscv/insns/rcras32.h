require_rv64;
P_CROSS_LOOP(32, {
  pd = (int32_t)(((int64_t)ps1 + (int64_t)ps2) >> 1);
}, {
  pd = (int32_t)(((int64_t)ps1 - (int64_t)ps2) >> 1);
})
