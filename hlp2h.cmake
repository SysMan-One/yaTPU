# Turn tpu.hlp into a C array of lines: cmake -DIN=tpu.hlp -DOUT=tpu_help.h -P hlp2h.cmake
file(READ "${IN}" content)
string(REPLACE "\\" "\\\\" content "${content}")
string(REPLACE "\"" "\\\"" content "${content}")
string(REGEX REPLACE "\n$" "" content "${content}")
string(REPLACE "\n" "\",\n\t\"" content "${content}")
file(WRITE "${OUT}" "/* Generated from tpu.hlp by hlp2h.cmake, do not edit */\nstatic const char *tpu_help_text[] = {\n\t\"${content}\",\n\tNULL\n};\n")
