# supergame-web-sim: shared/ compiled to a .wasm + glue .js for the browser's
# local mode (Phase 5). L35 wires the build; L36 replaces the stub with the
# Embind shim exposing GameSim::start()/step() over the JSON wire format.
add_executable(supergame-web-sim
    src/web_sim.cpp
)

target_link_libraries(supergame-web-sim PRIVATE
    supergame-shared
    supergame-warnings
)
