package(default_visibility = ["//visibility:public"])

COPTS = [ 
	"-g",
	"-O3",
]

cc_binary(
    name = "test",
    srcs = [
        "test.cpp",
    ],
    copts = COPTS,
    deps = [
        ":redis",
    ],
)

cc_library(
    name = "redis",
    hdrs = [
        "fde.h",
        "link.h",
        "link_addr.h",
        "Channel.h",
        "SelectableQueue.h",
        "Message.h",
        "Response.h",
        "Transport.h",
    ],
    srcs = [
        "fde.cpp",
        "link.cpp",
        "link_addr.cpp",
        "Message.cpp",
        "Response.cpp",
        "Transport.cpp",
    ],
    copts = COPTS,
    linkopts = [
        "-pthread"
    ],
)
