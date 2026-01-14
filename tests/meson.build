test_inc = include_directories('../src')

gtest_dep = dependency('gtest', fallback: 'gtest')
test_src = [
    'testsrc.cc'
]

test_exe = executable(
    'test_exe',
    sources: test_src,
    dependencies: [gtest_dep, gstpp_dep],
    include_directories: [test_inc],
)

test(
    'basic gtest suite',      # Name of the test case as shown by `meson test`
    test_exe                  # The executable to run
)