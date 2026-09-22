if(CROSSCOMPILING AND NOT EMULATOR)
  message("GKLIB_RUNTIME_SKIPPED: no cross-compiling emulator")
  return()
endif()

file(MAKE_DIRECTORY "${WORK_DIR}")
set(graph "${WORK_DIR}/valid.graph")
file(WRITE "${graph}" "2 1\n2\n1\n")
set(empty_graph "${WORK_DIR}/empty.graph")
file(WRITE "${empty_graph}" "0 0\n")
set(isolated_graph "${WORK_DIR}/isolated.graph")
file(WRITE "${isolated_graph}" "3 1\n2\n1\n\n")
set(csr_one_row "${WORK_DIR}/one-row.csr")
file(WRITE "${csr_one_row}" "0 1\n")
set(csr_two_rows "${WORK_DIR}/two-rows.csr")
file(WRITE "${csr_two_rows}" "0 1\n0 1\n")
set(duplicate_permutation "${WORK_DIR}/duplicate.perm")
file(WRITE "${duplicate_permutation}" "0\n0\n")
set(fis_matrix "${WORK_DIR}/fis.mat")
file(WRITE "${fis_matrix}" "1 2 2\n1 1 2 1\n")
set(extra_labels "${WORK_DIR}/extra.labels")
file(WRITE "${extra_labels}" "first\nsecond\nextra\n")
set(long_labels "${WORK_DIR}/long.labels")
string(REPEAT "x" 8192 long_label)
file(WRITE "${long_labels}" "${long_label}\nsecond\n")

function(expect_exit name expected)
  execute_process(
    COMMAND ${EMULATOR} ${ARGN}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error)
  if(expected STREQUAL "success")
    if(NOT result EQUAL 0)
      message(FATAL_ERROR
        "${name} unexpectedly failed (${result}):\n${output}${error}")
    endif()
  elseif(result EQUAL 0)
    message(FATAL_ERROR
      "${name} unexpectedly succeeded:\n${output}${error}")
  endif()
endfunction()

function(expect_failure_without_output name output_file)
  file(REMOVE "${output_file}")
  execute_process(
    COMMAND ${EMULATOR} ${ARGN}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error)
  if(result EQUAL 0)
    message(FATAL_ERROR
      "${name} unexpectedly succeeded:\n${output}${error}")
  endif()
  if(EXISTS "${output_file}")
    message(FATAL_ERROR
      "${name} left a plausible output file: ${output_file}")
  endif()
endfunction()

expect_exit(gkgraph-help success "${GKGRAPH}" -help)
expect_exit(gkgraph-missing-args failure "${GKGRAPH}")
expect_exit(gkgraph-malformed-integer failure "${GKGRAPH}"
  -niter=1junk "${graph}")
expect_exit(gkgraph-nonfinite-float failure "${GKGRAPH}"
  -eps=nan "${graph}")
expect_exit(gkgraph-valid success "${GKGRAPH}" "${graph}")
expect_exit(gkgraph-isolated success "${GKGRAPH}" "${isolated_graph}")
expect_exit(gkgraph-empty failure "${GKGRAPH}" "${empty_graph}")
expect_exit(csrcnv-malformed-format failure "${CSRCNV}"
  "${graph}" 0junk "${WORK_DIR}/unused.csr" 1)
expect_failure_without_output(csrcnv-duplicate-permutation
  "${WORK_DIR}/duplicate-output.csr" "${CSRCNV}"
  "-srenumber=${duplicate_permutation}" "${csr_two_rows}" 2
  "${WORK_DIR}/duplicate-output.csr" 2)
expect_exit(gkuniq-malformed-length failure "${GKUNIQ}" 10junk 2)
expect_exit(gkuniq-zero-length failure "${GKUNIQ}" 0 2)
expect_exit(gkuniq-zero-duplicates failure "${GKUNIQ}" 2 0)
expect_exit(gkuniq-overflow failure "${GKUNIQ}" 2147483647 2)
expect_exit(gkuniq-help success "${GKUNIQ}" -help)
expect_exit(cmpnbrs-malformed-verbosity failure "${CMPNBRS}"
  -verbosity=1junk)
expect_exit(cmpnbrs-row-mismatch failure "${CMPNBRS}"
  "${csr_one_row}" "${csr_two_rows}")
expect_exit(fis-malformed-minlen failure "${FIS}" -minlen=1junk)
expect_exit(fis-extra-label failure "${FIS}" "-clabels=${extra_labels}"
  "${fis_matrix}")
expect_exit(fis-long-label success "${FIS}" "-clabels=${long_labels}"
  "${fis_matrix}")
expect_exit(gkrw-malformed-niter failure "${GKRW}" -niter=1junk)
expect_failure_without_output(gkrw-ppr-zero
  "${WORK_DIR}/gkrw-ppr-zero.out" "${GKRW}" -ppr=0 "${graph}"
  "${WORK_DIR}/gkrw-ppr-zero.out")
expect_failure_without_output(gkrw-ppr-past-end
  "${WORK_DIR}/gkrw-ppr-past-end.out" "${GKRW}" -ppr=3 "${graph}"
  "${WORK_DIR}/gkrw-ppr-past-end.out")
expect_exit(grkx-malformed-input-format failure "${GRKX}"
  unused 1junk unused 1 1)
expect_failure_without_output(grkx-zero-copies
  "${WORK_DIR}/zero-copies.graph" "${GRKX}" "${graph}" 3
  "${WORK_DIR}/zero-copies.graph" 3 0)
expect_failure_without_output(grkx-negative-copies
  "${WORK_DIR}/negative-copies.graph" "${GRKX}" "${graph}" 3
  "${WORK_DIR}/negative-copies.graph" 3 -1)
expect_exit(m2mnbrs-malformed-count failure "${M2MNBRS}" -nnbrs=1junk)
expect_exit(m2mnbrs-missing-second-input failure "${M2MNBRS}" unused)
