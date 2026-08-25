# Fail the build if a symbol we asked objcopy to globalize is not global in
# the result. Called from tests/CMakeLists.txt; see the comment there for why
# this exists rather than trusting objcopy's exit status - objcopy succeeds
# happily when asked to globalize a name that is not present.
execute_process(COMMAND "${NM}" "${OBJ}" OUTPUT_VARIABLE out RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "emudiff: nm failed on ${OBJ}")
endif()
foreach(sym ${SYMS})
    if(NOT out MATCHES "[ \t]T[ \t]+${sym}\n")
        message(FATAL_ERROR
            "emudiff: '${sym}' is not a global symbol in ${OBJ}.\n"
            "  Either it does not exist in art.c under that name, or the "
            "compiler inlined it and discarded the out-of-line copy.\n"
            "  Do NOT work around this by editing art.c: check the name "
            "first, and prefer the debug preset where inlining is off.")
    endif()
endforeach()
# Data symbols land in a different nm class than functions: a globalized
# pointer in .bss is `B`, initialised data is `D`. Checking them against `T`
# would fail a correct build, and dropping them from the check would let a
# renamed global through silently - which is the exact hole this file exists
# to close.
foreach(sym ${DATA_SYMS})
    if(NOT out MATCHES "[ \t][BDGRS][ \t]+${sym}\n")
        message(FATAL_ERROR
            "emudiff: data symbol '${sym}' is not global in ${OBJ}.\n"
            "  Either it does not exist in art.c under that name, or it is "
            "not a data symbol. Check the name first; do NOT edit art.c.")
    endif()
endforeach()
message(STATUS "emudiff: globalized ${SYMS} ${DATA_SYMS}")
