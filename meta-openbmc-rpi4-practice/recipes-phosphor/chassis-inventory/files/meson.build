project(
    'chassis-inventory',
    'cpp',
    version: '1.0',
    default_options: [
        'cpp_std=c++23',
        'warning_level=3',
    ],
)

sdbusplus_dep = dependency('sdbusplus')
boost_dep = dependency('boost')

executable(
    'chassis-inventory',
    'main.cpp',
    dependencies: [
        sdbusplus_dep,
        boost_dep,
    ],
    install: true,
)
