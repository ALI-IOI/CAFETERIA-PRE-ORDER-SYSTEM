#pragma once
#include "httplib.h"
#include "server/app.h"

// Registers every /api route, the page guards and the security headers.
// devRoutes adds POST /api/dev/advance (only with --fake-clock, only from localhost).
void registerRoutes(httplib::Server& svr, AppState& app, bool devRoutes);
