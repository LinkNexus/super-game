# supergame-web-sim: shared/ compiled to a .wasm + ES-module glue for the
# browser's local mode (Phase 5). src/web_sim.cpp is the Embind shim (L36):
# a WebSim class, JSON PlayerInputs in, the server's {type, payload} state
# envelope out.
add_executable(supergame-web-sim
    src/web_sim.cpp
)

target_link_libraries(supergame-web-sim PRIVATE
    supergame-shared
    supergame-warnings
)

# MODULARIZE + EXPORT_ES6: `import createModule from "./supergame-web-sim.mjs"`
# then `await createModule()`, rather than a global `Module`. web for the
# frontend, node for the ctest in tests/.
set_target_properties(supergame-web-sim PROPERTIES SUFFIX ".mjs")
target_link_options(supergame-web-sim PRIVATE
    -lembind
    -sMODULARIZE=1
    -sEXPORT_ES6=1
    -sENVIRONMENT=web,node
    -sALLOW_MEMORY_GROWTH=1
    # TS types for the module + the Embind API (WebSim), for web/frontend.
    # .d.mts: TS only pairs an .mjs import with a .d.mts. The JSON payloads
    # stay strings here; their TS types are web/frontend/src/messages.ts.
    "SHELL:--emit-tsd $<TARGET_FILE_DIR:supergame-web-sim>/supergame-web-sim.d.mts"
)
