idf_component_register(
  INCLUDE_DIRS "includes"
  SRC_DIRS "." "gnuboy"
  PRIV_INCLUDE_DIRS "gnuboy"
  )

component_compile_options(-fjump-tables -ftree-switch-conversion -O3)
target_compile_options(${COMPONENT_LIB} PRIVATE -nostartfiles -nostdlib -fno-builtin -Wno-misleading-indentation -Wno-implicit-fallthrough -Wno-unused-function -Wno-unused-variable -Wno-discarded-qualifiers)
target_compile_definitions(${COMPONENT_LIB} PRIVATE GNUBOY_NO_MINIZIP GNUBOY_NO_SCREENSHOT IS_LITTLE_ENDIAN)
