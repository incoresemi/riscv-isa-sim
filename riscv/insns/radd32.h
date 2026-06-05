require_rv64;
P_LOOP(32, {
  pd = (int32_t)(((int64_t)ps1 + (int64_t)ps2) >> 1);
})
