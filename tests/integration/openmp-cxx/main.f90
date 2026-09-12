! Execute a CSR normalization region without declaring the C structure layout.
program consumer
  use, intrinsic :: iso_c_binding
  implicit none
  interface
    function read_matrix(filename, format, values, numbering) bind(C, name="gk_csr_Read") result(matrix)
      import :: c_char, c_int, c_ptr
      character(kind=c_char) :: filename(*)
      integer(c_int), value :: format, values, numbering
      type(c_ptr) :: matrix
    end function
    subroutine normalize_matrix(matrix, what, norm) bind(C, name="gk_csr_Normalize")
      import :: c_ptr, c_int
      type(c_ptr), value :: matrix
      integer(c_int), value :: what, norm
    end subroutine
    subroutine write_matrix(matrix, filename, format, values, numbering) bind(C, name="gk_csr_Write")
      import :: c_char, c_int, c_ptr
      type(c_ptr), value :: matrix
      character(kind=c_char) :: filename(*)
      integer(c_int), value :: format, values, numbering
    end subroutine
    subroutine free_matrix(matrix) bind(C, name="gk_csr_Free")
      import :: c_ptr
      type(c_ptr) :: matrix
    end subroutine
    function begin_allocations() bind(C, name="gk_malloc_init") result(status)
      import :: c_int
      integer(c_int) :: status
    end function
    subroutine end_allocations(stats) bind(C, name="gk_malloc_cleanup")
      import :: c_int
      integer(c_int), value :: stats
    end subroutine
    function replace_text(text, pattern, replacement, options, output) &
        bind(C, name="gk_strstr_replace") result(status)
      import :: c_char, c_ptr, c_int
      character(kind=c_char) :: text(*), pattern(*), replacement(*), options(*)
      type(c_ptr) :: output
      integer(c_int) :: status
    end function
  end interface
  type(c_ptr) :: matrix, output
  character(kind=c_char), pointer :: result_text(:)
  integer(c_int) :: status
  integer :: unit, first, second
  real(c_float) :: x, y
  ! Public GK_CSR_FMT_CSR=2 and GK_CSR_ROW=1 constants are independent of ABI width.
  open(newunit=unit, file='consumer-input.csr', status='replace')
  write(unit, '(A)') '0 3 1 4'
  close(unit)
  matrix = read_matrix('consumer-input.csr' // c_null_char, 2_c_int, 1_c_int, 0_c_int)
  if (.not. c_associated(matrix)) stop 1
  call normalize_matrix(matrix, 1_c_int, 2_c_int)
  call write_matrix(matrix, 'consumer-output.csr' // c_null_char, 2_c_int, 1_c_int, 0_c_int)
  call free_matrix(matrix)
  if (c_associated(matrix)) stop 2
  open(newunit=unit, file='consumer-output.csr', status='old')
  read(unit, *) first, x, second, y
  close(unit, status='delete')
  if (first /= 0 .or. second /= 1) stop 6
  if (abs(x - 0.6_c_float) > 1e-6_c_float .or. abs(y - 0.8_c_float) > 1e-6_c_float) stop 7
  open(newunit=unit, file='consumer-input.csr', status='old')
  close(unit, status='delete')

  ! Exercise the selected regex runtime through GKlib, preserving its allocation
  ! ownership without declaring a Fortran representation of POSIX regex_t.
  if (begin_allocations() /= 1) stop 3
  status = replace_text('foo' // c_null_char, '^foo$' // c_null_char, &
    'bar' // c_null_char, c_null_char, output)
  if (status /= 2 .or. .not. c_associated(output)) stop 4
  call c_f_pointer(output, result_text, [4])
  if (any(result_text /= [character(kind=c_char) :: 'b', 'a', 'r', c_null_char])) stop 5
  call end_allocations(0_c_int)
end program
