M4 Source Code

m4_define([DATE],['`date +%Y-%m-%d`'])

m4_include([config/ax_compiler_vendor.m4])

m4_include([config/pagesize.m4])
m4_include([config/madvise-flags.m4])
m4_include([config/mmap-flags.m4])
m4_include([config/acx_mmap_fixed.m4])
m4_include([config/acx_mmap_variable.m4])
m4_include([config/shm-flags.m4])

m4_include([config/ax_mpi.m4])
m4_include([config/acx_pthread.m4])

m4_include([config/struct-stat64.m4])
m4_include([config/expand.m4])
m4_include([config/perl.m4])

m4_include([config/fopen.m4])
m4_include([config/asm-bsr.m4])
m4_include([config/sse2_slli_const_imm8.m4])
m4_include([config/sttni.m4])

m4_include([config/builtin-popcount.m4])
m4_include([config/simd-intrinsics.m4])

m4_include([config/ax_cpuid_intel.m4])
m4_include([config/ax_cpuid_non_intel.m4])
m4_include([config/ax_check_compile_flag.m4])
m4_include([config/ax_ext.m4])

