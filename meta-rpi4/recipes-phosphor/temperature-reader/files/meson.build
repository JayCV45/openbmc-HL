project(
    'temperature-reader',
    'cpp',
    version: '1.0',
    default_options: [
        'cpp_std=c++23',
        'warning_level=3',
    ],
)

# OpenBMC D-Bus library.
sdbusplus_dep = dependency('sdbusplus')

# Boost.Asio headers.
boost_dep = dependency('boost')

executable(
    'temperature-reader',
    'main.cpp',
    dependencies: [
        sdbusplus_dep,
        boost_dep,
    ],
    install: true,
)