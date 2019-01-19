set(COMPONENT_ADD_INCLUDEDIRS include)
set(COMPONENT_SRCS "src/darksky.c")

set(COMPONENT_REQUIRES esp-tls json)

set(COMPONENT_EMBED_TXTFILES server_root_cert.pem)

register_component()
